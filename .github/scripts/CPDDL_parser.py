import re

from lab.parser import Parser

def coverage(content, props):
    props["coverage"] = int(props["planner_exit_code"] == 0)


def get_plan(content, props):
    # All patterns are parsed before functions are called.
    if props.get("evaluations") is not None:
        props["plan"] = re.findall(r"^(?:step)?\s*\d+: (.+)$", content, re.M)
        props["found_plan_length"] = len(props["plan"])


def get_times(content, props):
    log_times = re.findall(r"\[(\d+(?:\.\d+)?)s(?:\s|\])", content)
    second_times = re.findall(r"(\d+(?:\.\d+)?) seconds", content)

    times = log_times if log_times else second_times
    props["times"] = times
    props["time_total"] = float(times[-1]) if times else None

    search_match = re.search(r"Search time:\s*(\d+(?:\.\d+)?)s", content)
    total_match = re.search(r"Total time:\s*(\d+(?:\.\d+)?)s", content)
    if search_match:
        props["search_time"] = float(search_match.group(1))
    if total_match:
        props["total_time"] = float(total_match.group(1))


def trivially_unsolvable(content, props):
    props["trivially_unsolvable"] = int(
        "CPDDL_planner: goal can be simplified to FALSE. No plan will solve it" in content
    )



def error(content, props):
# Safely get the exit code, defaulting to None if it wasn't found
    exit_code = props.get("planner_exit_code")

    if exit_code == 0:
        props["error"] = "plan-found"
    elif exit_code is None:
        props["error"] = "missing-exit-code" 
    else:
        props["error"] = "unsolvable-or-error"

def coverage(content, props):
    # Safely check if the exit code is exactly 0
    props["coverage"] = int(props.get("planner_exit_code") == 0)


class CPDDLParser(Parser):
    def __init__(self):
        super().__init__()
        self.add_pattern(
            "node", r"node: (.+)\n", type=str, file="driver.log", required=True
        )
        self.add_pattern(
            "planner_exit_code",
            r"run-planner exit code: (.+)\n",
            type=int,
            file="driver.log",
        )
        self.add_pattern("evaluations", r"evaluating (\d+) states")
        self.add_function(error)
        self.add_function(coverage)
        self.add_function(get_plan)
        self.add_function(get_times)
        self.add_function(trivially_unsolvable)
