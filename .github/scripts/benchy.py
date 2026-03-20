import os
import time
import socket
import subprocess
import requests
from typing import Optional
from lab.parser import Parser
import json


INFO_SECRET = os.getenv("DISCORD_INFO_WEBHOOK")
ALERT_SECRET = os.getenv("DISCORD_ALERT_WEBHOOK")
OVERVIEW_SECRET = os.getenv("DISCORD_OVERVIEW_WEBHOOK")
    

class DiscordJobNotifier:
    def __init__(self, overview_url: str, info_url: str, alert_url: str, job_id: str):
        self.overview_base = overview_url
        self.info_url = info_url
        self.alert_url = alert_url
        self.hostname = socket.gethostname()
        self.job_id = job_id
        self.card_id: Optional[str] = None

    def _send_log(self, url: str, message: str, emoji: str):
        payload = {"content": f"{emoji} **[Job {self.job_id}]**: {message}"}
        try:
            requests.post(url, json=payload, timeout=10)
        except requests.exceptions.RequestException as e:
            print(f"Failed to send log: {e}")

    def log_info(self, message: str): self._send_log(self.info_url, message, "ℹ️")
    def log_alert(self, message: str): self._send_log(self.alert_url, message, "🚨")

    def create_status_card(self, text: str):
        payload = {
            "embeds": [{
                "title": "Slurm Job Update",
                "description": text,
                "color": 5814783,
                "footer": {"text": f"Submitted from: {self.hostname} | Job: {self.job_id}"}
            }]
        }
        try:
            r = requests.post(f"{self.overview_base}?wait=true", json=payload, timeout=10)
            r.raise_for_status()
            self.card_id = r.json().get('id')
        except requests.exceptions.RequestException as e:
            print(f"Error creating card: {e}")

    def update_status_card(self, text: str, color: int = 5814783):
        if not self.card_id: return
        payload = {
            "embeds": [{
                "title": "Slurm Job Update",
                "description": text,
                "color": color,
                "footer": {"text": f"Submitted from: {self.hostname} | Job: {self.job_id}"}
            }]
        }
        try:
            requests.patch(f"{self.overview_base}/messages/{self.card_id}", json=payload, timeout=10)
        except requests.exceptions.RequestException as e:
            print(f"Error updating card: {e}")

    def upload_output(self, filepath: str):
        if not os.path.exists(filepath):
            self.log_alert(f"File {filepath} not found for upload.")
            return
        try:
            with open(filepath, 'rb') as f:
                files = {'file': (os.path.basename(filepath), f)}
                requests.post(self.info_url, files=files, timeout=20)
        except Exception as e:
            print(f"Error uploading file: {e}")


def get_job_state(job_id: str) -> str:
    """Queries Slurm for the current state of the job."""
    try:
        # sacct returns states like PENDING, RUNNING, COMPLETED, FAILED
        result = subprocess.run(
            ["sacct", "-j", job_id, "-o", "State", "-n", "-X"], 
            capture_output=True, text=True, check=True
        )
        states = result.stdout.strip().split()
        return states[0] if states else "UNKNOWN"
    except subprocess.CalledProcessError:
        return "UNKNOWN"

def main():
    parser = Parser()

    try:
        print("Submitting job.sh to Slurm...")
        result = subprocess.run(["sbatch", "../../job.sh"], capture_output=True, text=True, check=True)
        job_id = result.stdout.strip().split()[-1]
    except subprocess.CalledProcessError as e:
        print(f"Failed to submit job: {e.stderr}")
        return

    notifier = DiscordJobNotifier(
        overview_url=OVERVIEW_SECRET,
        info_url=INFO_SECRET,
        alert_url=ALERT_SECRET,
        job_id=job_id
    )

    notifier.log_info(f"Job submitted to queue.")
    notifier.create_status_card(f"Job is currently **PENDING** in the Slurm queue.")

    last_state = "PENDING"
    while True:
        time.sleep(30) 
        current_state = get_job_state(job_id)

        if current_state != last_state:
            if current_state == "RUNNING":
                notifier.update_status_card(f"Job has started **RUNNING** on the cluster.", color=16753920)
            last_state = current_state

        if current_state in ["COMPLETED", "FAILED", "CANCELLED", "TIMEOUT"]:
            break

    if current_state == "COMPLETED":
        notifier.update_status_card(f"✅ Job **COMPLETED** successfully.", color=3066993)
        notifier.log_info("Uploading output files...")
        notifier.upload_output("data_plan.out")
            
        parser.add_pattern('search_time', r'Search time: (.*)s', type=float, required=False, file="data_plan.out")
        parser.add_pattern('total_time', r'Total time: (.*)s', type=float, required=False, file="data_plan.out")
        parser.add_pattern('plan_length', r'Plan length: (.*) step\(s\)', type=int, required=False, file="data_plan.out")
        parser.add_pattern('expanded_nodes', r'Expanded (.*) state\(s\)', type=int, required=False, file="data_plan.out")
        parser.add_pattern('evaluated_states', r'Evaluated (.*) state\(s\)', type=int, required=False, file="data_plan.out")
        parser.add_pattern('solution_found', r'(Solution found!)', type=str, required=False, file="data_plan.out")

        parser.add_pattern('solution_found', r'(Solution found!)', type=str, required=False)


        props = {}
        
        results = parser.parse(".", props)

        with open("parsed_results.json", "w") as f:
            json.dump(results, f, indent=4)

    else:
        notifier.update_status_card(f"❌ Job ended with status: **{current_state}**.", color=15158332)
        notifier.log_alert("Job failed or was cancelled. Check error logs.")
        notifier.upload_output("data_plan.err")

if __name__ == "__main__":
    main()