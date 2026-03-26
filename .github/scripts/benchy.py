import os
import socket
import shutil
import requests
from typing import Optional
import pathlib

from CPDDL_parser import CPDDLParser
from report_prettifier import style_all_generated_reports

from downward import suites
from downward.reports.absolute import AbsoluteReport
from lab.environments import SlurmEnvironment
from lab.experiment import Experiment
from lab.reports import Attribute, geometric_mean


BASE_PATH = pathlib.Path(__file__).parent.resolve()

INFO_SECRET = os.getenv("DISCORD_INFO_WEBHOOK")
ALERT_SECRET = os.getenv("DISCORD_ALERT_WEBHOOK")
OVERVIEW_SECRET = os.getenv("DISCORD_OVERVIEW_WEBHOOK")



TIME_LIMIT = int(os.getenv("BENCHY_TIME_LIMIT", "300"))
MEMORY_LIMIT = int(os.getenv("BENCHY_MEMORY_LIMIT", str(8 * 1024)))
CPU_PER_TASK = int(os.getenv("BENCHY_CPU_PER_TASK", "1"))
SLURM_PARTITION = os.getenv("SLURM_PARTITION", "l4")
SLURM_QOS = os.getenv("SLURM_QOS")

SUITES = [
    "barman14:p435.1.pddl",
]

ALGORITHMS = [
    {
        "name": "hmax",
        "planner_args": ["--gplan", "astar", "--gplan-h", "hmax"],
    },
    {
        "name": "lmc",
        "planner_args": ["--gplan", "astar", "--gplan-h", "lmc"],
    },
]

ATTRIBUTES = [
    "error",
    "plan",
    "found_plan_length",
    "search_time",
    "total_time",
    "time_total",
    "times",
    Attribute("coverage", absolute=True, min_wins=False, scale="linear"),
    Attribute("evaluations", function=geometric_mean),
    Attribute("trivially_unsolvable", min_wins=False),
]



class DiscordJobNotifier:
    def __init__(self, overview_url: str, info_url: str, alert_url: str, job_id: str):
        self.overview_base = overview_url
        self.info_url = info_url
        self.alert_url = alert_url
        self.hostname = socket.gethostname()
        self.job_id = job_id
        self.card_id: Optional[str] = None

    def _send_log(self, url: str, message: str):
        if not url:
            return
        payload = {"content": f"[Job {self.job_id}] {message}"}
        try:
            requests.post(url, json=payload, timeout=10)
        except requests.exceptions.RequestException as e:
            print(f"Failed to send log: {e}")

    def log_info(self, message: str): self._send_log(self.info_url, message)
    def log_alert(self, message: str): self._send_log(self.alert_url, message)

    def create_status_card(self, text: str):
        if not self.overview_base:
            return
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
        if not self.card_id or not self.overview_base:
            return
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



class BenchyRunner:
    class BaseReport(AbsoluteReport):
        INFO_ATTRIBUTES = ["time_limit", "memory_limit"]
        ERROR_ATTRIBUTES = [
            "domain",
            "problem",
            "algorithm",
            "unexplained_errors",
            "error",
            "node",
        ]

    def __init__(self, base_path: pathlib.Path):
        self.base_path = base_path
        self.repo_root = self._resolve_repo_root()
        self.benchmarks_dir = self._resolve_benchmarks_dir()
        self.planner_bin = self._resolve_planner_bin()
        self.time_limit = TIME_LIMIT
        self.memory_limit = MEMORY_LIMIT
        self.cpu_per_task = CPU_PER_TASK
        self.notifier = DiscordJobNotifier(
            overview_url=OVERVIEW_SECRET,
            info_url=INFO_SECRET,
            alert_url=ALERT_SECRET,
            job_id=f"benchy-{os.getpid()}",
        )

        if shutil.which("sbatch") is None:
            raise RuntimeError(
                "Slurm is required, but sbatch is not available on PATH."
            )

        env_kwargs = {
            "partition": SLURM_PARTITION,
            "email": "dz30ed@student.aau.dk",
            "memory_per_cpu": f"{self.memory_limit}M",
            "cpus_per_task": self.cpu_per_task,
        }
        if SLURM_QOS:
            env_kwargs["qos"] = SLURM_QOS
        self.env = SlurmEnvironment(**env_kwargs)
        
        if not SLURM_QOS:
            self.env.qos = None 

        self.suites = SUITES

        self.suites = SUITES
        self.algorithms = ALGORITHMS
        self.attributes = ATTRIBUTES
        self.exp = Experiment(environment=self.env)
        self.exp.add_parser(CPDDLParser())

    def _resolve_repo_root(self) -> pathlib.Path:
        github_workspace = os.getenv("GITHUB_WORKSPACE")
        if not github_workspace:
            raise RuntimeError("GITHUB_WORKSPACE is required in runner mode.")
        root = pathlib.Path(github_workspace)
        workflow_file = root / ".github" / "workflows" / "benchmark.yml"
        if not workflow_file.is_file():
            raise FileNotFoundError(
                f"Expected runner workspace layout not found. Missing: {workflow_file}"
            )
        return root

    def _resolve_benchmarks_dir(self) -> str:
        candidates = [
            self.repo_root / "pddl-data" / "bench" / "ipc-opt-noce",
            self.repo_root / "pddl-data" / "ipc-opt-noce",
        ]
        for candidate in candidates:
            if candidate.is_dir():
                return str(candidate)
        searched = ", ".join(str(path) for path in candidates)
        raise FileNotFoundError(f"Could not find benchmark directory. Checked: {searched}")

    def _resolve_planner_bin(self) -> str:
        planner = self.repo_root / "bin" / "pddl"
        if planner.is_file():
            return str(planner)
        raise FileNotFoundError(
            f"Could not find planner binary at {planner}. "
            "Expected bin/ to be provided by compile artifact."
        )

    def _add_runs(self):
        for task in suites.build_suite(self.benchmarks_dir, self.suites):
            for algorithm in self.algorithms:
                run = self.exp.add_run()
                problem_name = pathlib.Path(task.problem_file).name
                algorithm_name = algorithm["name"]
                planner_args = algorithm["planner_args"]

                run.add_resource("domain", task.domain_file, symlink=True)
                run.add_resource("problem", task.problem_file, symlink=True)
                run.add_command(
                    "run-planner",
                    [
                        self.planner_bin,
                        *planner_args,
                        "{domain}",
                        "{problem}",
                    ],
                    time_limit=self.time_limit,
                    memory_limit=self.memory_limit,
                )
                run.set_property("domain", task.domain)
                run.set_property("problem", problem_name)
                run.set_property("algorithm", algorithm_name)
                run.set_property("time_limit", self.time_limit)
                run.set_property("memory_limit", self.memory_limit)
                run.set_property("id", [algorithm_name, task.domain, problem_name])

    def _add_steps_and_reports(self):
        self.exp.add_step("build", self.exp.build)
        self.exp.add_step("start", self.exp.start_runs)
        self.exp.add_step("parse", self.exp.parse)
        self.exp.add_fetcher(name="fetch")
        self.exp.add_report(self.BaseReport(attributes=self.attributes), outfile="report.html")
        for algorithm in self.algorithms:
            algorithm_name = algorithm["name"]
            self.exp.add_report(
                self.BaseReport(
                    attributes=self.attributes,
                    filter_algorithm=[algorithm_name],
                ),
                outfile=f"report-{algorithm_name}.html",
            )

    def run(self):
        self.notifier.create_status_card("Benchy experiment queued.")
        self.notifier.log_info(
            f"Starting benchmark run: env=slurm, suites={len(self.suites)}, "
            f"algorithms={len(self.algorithms)}"
        )
        print(
            f"[Benchy] Environment: slurm; benchmarks_dir={self.benchmarks_dir}; "
            f"planner_bin={self.planner_bin}"
        )
        try:
            self._add_runs()
            self._add_steps_and_reports()
            self.exp.run_steps()
            style_all_generated_reports(self.base_path)

            report_files = sorted(
                (self.base_path / "data").glob("*-eval/report*.html")
            )
            for report_path in report_files:
                self.notifier.upload_output(str(report_path))
            self.notifier.update_status_card(
                "Benchy experiment completed successfully.",
                color=3066993,
            )
            self.notifier.log_info("Benchy reports generated.")
        except Exception as exc:
            self.notifier.update_status_card(
                f"Benchy experiment failed: {exc}",
                color=15158332,
            )
            self.notifier.log_alert(f"Benchy experiment failed: {exc}")
            raise


if __name__ == "__main__":
    BenchyRunner(BASE_PATH).run()
