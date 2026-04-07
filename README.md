# Ghost-2077: Advanced Endpoint Threat & Malware Analysis PoC

**⚠️ STRICTLY FOR EDUCATIONAL & RESEARCH PURPOSES ONLY ⚠️**

Hey everyone! Welcome to Ghost-2077. Just a quick heads-up: this is a personal project I built entirely on my own from scratch. 

I created this Proof of Concept (PoC) simply because I have a huge curiosity about how things work under the hood. I wanted to push my own limits and explore the actual mechanics of how sophisticated Remote Access Trojans (RATs) and offensive tools operate. 

**Why did I build this?**
I strongly believe that to build a solid defense, we need to understand how the offense plays the game. By diving deep into modern malware and evasion techniques, my goal is to learn how to:
* Spot these kinds of threats more effectively.
* Improve endpoint monitoring and detection.
* Understand what makes a good Incident Response playbook.

**The Serious Disclaimer (Please Read!):**
Let me be perfectly clear: I am sharing this codebase **strictly** for fellow students, security researchers, and anyone who wants to learn. 

I take **ABSOLUTELY ZERO RESPONSIBILITY** for how you use this code. Please be smart and ethical. Do **NOT** compile, deploy, or run this project on any system, network, or machine unless you own it or have explicit, authorized permission to do so.

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

## 🚀 Building the Project

*(Note: Do not compile or build this outside of a controlled, isolated virtual machine environment).*

For safety and to prevent automated or accidental execution by malicious actors, the build script (`build.bat`) and all compiled binaries are intentionally **NOT** included in this repository. 

If you are a security researcher or malware analyst looking to study this PoC, you must manually assemble and compile the project using the **Developer Command Prompt for Visual Studio**.

**Manual Compilation Guidelines:**
1. **Resource Compilation:** First, use the Microsoft Resource Compiler (`rc.exe`) to compile the payload resource file located at `src\loader\resource.rc` into a `.res` object.
2. **Source Compilation:** Use the MSVC Compiler (`cl.exe`) configured for the C++17 standard (`/std:c++17`). You will need to compile all `.cpp` files located within the `src/` directories (`core`, `persistence`, `loader`, `crypto`, `collector`, and `c2`).
3. **Linking Dependencies:** During compilation, ensure you link the following native Win32 libraries for the modules to function correctly:
   * **Core & System:** `user32.lib`, `advapi32.lib`, `shell32.lib`, `shlwapi.lib`, `psapi.lib`
   * **Networking & C2:** `winhttp.lib`, `dnsapi.lib`, `ws2_32.lib`
   * **Cryptography:** `bcrypt.lib`
   * **Collector/Screen Capture:** `windowscodecs.lib`, `d3d11.lib`, `dxgi.lib`, `ole32.lib`, `gdi32.lib`

*Self-assembly is strictly required to ensure this code is only utilized by individuals who fully understand its mechanics and implications.*

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
