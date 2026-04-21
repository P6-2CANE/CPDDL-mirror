#!/bin/bash

#SBATCH --job-name=data_plan
#SBATCH --output=data_plan.out
#SBATCH --error=data_plan.err
#SBATCH --time=00:30:00
#SBATCH --mem=4G
#SBATCH --cpus-per-task=2
#SBATCH --gres=gpu:1

hostname

echo 'Running data plan from AI-LAB'



curl -sL https://api.github.com/repos/P6-2CANE/CPDDL-mirror/releases/latest | grep "browser_download_url" | cut -d '"' -f 4 | xargs -n 1 curl -LO

chmod +x /ceph/project/2CANE/pddl

./pddl --gplan astar --gplan-h hmax ./pddl-data/bench/ipc-opt-noce/barman14/domain.pddl ./pddl-data/bench/ipc-opt-noce/barman14/p435.2.pddl