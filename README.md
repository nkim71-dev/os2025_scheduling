# Operating Systems Scheduling Experiment Guide

**Apply and analyze Unix-style scheduling algorithms learned in class on a real Linux system (Ubuntu).**


---
---

## 1. Environment Setup

### 1.1 Install Required Packages

```bash
sudo apt update
sudo apt install -y build-essential htop util-linux
```

- **build-essential**: gcc and related build tools
- **htop**: interactive process monitor for visualizing scheduling behavior
- **util-linux**: provides `chrt` commands

---
---
## 2. Compiling and Running `my_program`

### 2.2 Compile the Program

```bash
gcc my_program.c -o my_program 
```

### 2.2. Basic Usage

```bash
./my_program [mode] [label] [seconds]
```

- mode: cpu or io
- label: text label for identification in logs
- seconds: duration (optinal, default 20 seconds)


Example:
```bash
./my_program cpu my_program_CFS 30 
```

---
---

## 3. Two-Terminal Setup

Use two separate terminals to effectively monitor scheduling behavior:

### 3.1 Terminal 1 — Monitoring (htop)

```bash
htop
```

**Configuration:**
1. Press **F4** and type `my_program` to filter experimental processes
2. Press **F2** → **Columns** and add these fields:
   - PID, USER, PRI, NI, STATE, etc.

**What to observe:**
- Applied scheduling policy (using label)
- State (R: running, S: sleeping, T: stopped, etc.)
- CPU Time (user mode, kernel mode, overall, etc.)
- Priority

### 3.2 Terminal 2 — Execution (running `my_program`)

Run programs with different scheduling policies.

⚠️ **Important**: 
- Use `&` at the end to run the program in background and run multiple processes simultaneously if needed.
- Run for at least **20-30 seconds** to observe noticeable scheduling differences.



#### Examples:

```bash
# CFS (default policy)
./my_program cpu CFS 30 &

# RR (Round Robin)
sudo chrt -r 50 ./my_program cpu RR 30 &

# FIFO (First-In First-Out)
sudo chrt -f 50 ./my_program cpu FIFO 30 &
```

Monitor behavior differences in htop: CPU time distribution, priority, and scheduling order.


#### ⚠️ **About CFS** (Completely Fair Scheduler)

CFS is Linux's default scheduler designed to ensure **fairness** among all runnable processes.

- Each process maintains a **virtual runtime (vruntime)** tracking effective CPU time received
- High CPU usage → vruntime grows faster → scheduled less frequently
- I/O-bound tasks → slower vruntime growth → scheduled more often
- Combines ideas from MLFQ and Stride/Lottery Scheduling into a mathematically structured model


#### ⚠️ **About RR (Round Robin) and FIFO (First-In First-Out)**

Real-time scheduling policies for RR and FIFO:
- Use `chrt` command with `-r` (RR) or `-f` (FIFO) option
   - Requires **root privileges** to set
- Priority range: **1 (lowest)** to **99 (highest)**
   - Higher priority processes **always preempt** lower priority ones
   - Real-time processes have priority over all CFS processes


**Difference between RR and FIFO:**
- **RR (Round Robin)**: Processes with same priority share CPU in time slices
- **FIFO (First-In First-Out)**: Process runs until it blocks or yields

---
---

## 4. CPU-bound vs IO-like Workloads

### 4.1 Running Different Workload Types

Compare different workload types:

```bash
# CPU-bound mode
./my_program cpu CPU 30 &

# IO-like mode
./my_program io IO 30&
```

### 4.2 What to Compare

- Printed loop count (loops/sec)
- CPU utilization in htop
- I/O-bound tasks yield CPU more often
- CPU-bound tasks monopolize CPU longer
- ETC.

---
---

## 5. Effect of Priority

### 5.1 RR / FIFO (Real-time Priority)


#### Examples:

```bash
sudo chrt -r 20 ./my_program cpu RR_LOW 30 &
sudo chrt -r 80 ./my_program cpu RR_HIGH 30 &
```

Observe how higher-priority processes preempt lower-priority ones.

### 5.2 CFS (Nice Value)


#### Examples:

```bash
nice -n 0 ./my_program cpu NICE_0 30 &
nice -n 10 ./my_program cpu NICE_10 30 &
```

Check the **NI (nice)** column in htop to see which process runs more frequently.


#### ⚠️ **About Nice Values**
For CFS, priority is adjusted with **nice values**:
- Nice values range from **-20 (highest priority)** to **+19 (lowest priority)**
   - Lower nice values → higher priority → more CPU time
   - Higher nice values → lower priority → less CPU time
   - Default nice value is **0**
   - Only root can set negative nice values
   - Real-time processes have priority over all CFS processes


---
---

## 6. Additional Experiments (Optional)
- Explore various combinations of scheduling policies and priorities.
- Derive and summarize key findings and insights from your observations.

⚠️ **Important**: This part is optional, but it will count toward the grade.

---
---

## 7. Report

### 7.1 Submission Requirements

- Use the provided LaTeX or Word template
- Submit final report as **PDF (maximum 3 pages)**
- Write in English or Korean

### 7.2 Recommended Contents

- Run experiments for sufficient duration (20-30 seconds minimum)
- Take screenshots of htop during different scheduling scenarios
- Document commands and their outputs with key findings
- Compare and contrast different scheduling policies clearly
