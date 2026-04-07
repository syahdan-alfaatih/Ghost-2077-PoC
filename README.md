# Ghost-2077: Advanced Endpoint Threat & Malware Analysis PoC

⚠️ **STRICTLY FOR EDUCATIONAL & RESEARCH PURPOSES ONLY** ⚠️

This repository contains a Proof of Concept (PoC) demonstrating techniques commonly found in sophisticated Remote Access Trojans (RATs) and endpoint enumeration tools.

---

## Disclaimer

This project was developed strictly for **academic research** and to understand the inner workings of modern malware, evasion techniques, and offensive toolchains.

Understanding these offensive mechanics is essential for:

* Building robust defensive strategies
* Improving endpoint monitoring
* Developing Incident Response playbooks
* Security research and malware analysis

The author assumes **NO responsibility** for any misuse.
Do **NOT** compile or deploy this project on any system without **explicit authorization**.

---

# 🛠️ Tech Stack & Compilation

## Language

* C++17

## Compiler

* MSVC (`cl.exe`) for optimized Windows-native execution

## Core Dependencies

* Win32 API
* `Advapi32`
* `WinHTTP`
* `DNSAPI`
* `BCrypt`
* `DXGI` / `D3D11`

## Build System

* Custom Batch Script using Windows SDK

---

# ⚙️ Architecture & Capabilities Explored

## 1. Command & Control (C2) Infrastructure

Demonstrates multiple exfiltration and beaconing channels designed to bypass network monitoring:

* HTTPS communication using WinHTTP
* DNS tunneling via `alwaysdata[.]net` relays
* TOR network onion routing for anonymized beaconing

---

## 2. Data Collection & Harvester Modules

Explores techniques commonly used by InfoStealers to gather sensitive data:

* Keystroke logging via Windows Hooks
* Credential dumping from browsers (Chrome, Edge, Firefox)
* LSASS / DPAPI interaction concepts
* Screen capture using D3D11 / DXGI
* File enumeration across user directories
* Email store discovery
* Messenger cache discovery (WhatsApp)

---

## 3. Persistence & Privilege Escalation

Techniques for maintaining system access:

* Registry Run Key injection
* Scheduled Task persistence
* UAC bypass techniques
* BITSAdmin persistence via Background Intelligent Transfer Service

---

## 4. Defense Evasion

Implemented concepts include:

* Payload loading via resource compilation (`.res` / `.rc`)
* Hardcoded string obfuscation
* Encrypted buffer handling
* Static detection evasion concepts

---

# 🚀 Building the Project

⚠️ **Only build inside an isolated virtual machine environment**

A `build.bat` script is provided to compile the project using Developer Command Prompt for Visual Studio:

```bat
build.bat
```

This script will:

* Compile payload resources using `rc.exe`
* Compile main project using `cl.exe`
* Link required Win32 libraries

---

# 📁 Project Structure

```
Ghost-2077/
│
├── src/                # Core source files
├── include/            # Headers
├── resources/          # Payload resources (.rc / .res)
├── build.bat           # Build script
├── LICENSE             # License file
└── README.md           # Documentation
```

---

# 🔬 Research Scope

This PoC is intended for:

* Malware analysis learning
* Red team research
* Blue team detection engineering
* Threat modeling exercises
* EDR testing in lab environments

---

# 📄 License

This project is licensed under a **Custom Educational Use Only License**.

The following are strictly prohibited:

* Commercial use
* Malicious deployment
* Unauthorized distribution
* Real-world system execution without permission

See the `LICENSE` file for full details.
