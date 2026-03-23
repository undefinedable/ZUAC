# ZUAC (Zeppelin UAC)

Pure C implementation of a UAC bypass technique leveraging environment variable manipulation and the Windows Task Scheduler.

Author: @undefinedable

---

## Overview

ZUAC implements a fileless User Account Control (UAC) bypass technique based on the abuse of the **SilentCleanup** scheduled task.

The technique relies on the fact that certain scheduled tasks:
- Are configured with **RunLevel = Highest (auto-elevated)**
- Can be triggered by a standard user
- Reference **environment variables** (e.g., `%windir%`) in their execution path

By overriding user-controlled environment variables, execution flow can be redirected to an arbitrary payload, resulting in elevated execution without prompting the user.

This behavior originates from design decisions in Task Scheduler and environment variable resolution, where user-controlled registry values may influence privileged execution paths. :contentReference[oaicite:0]{index=0}

---

## Technical Details

### SilentCleanup Task Abuse

The target task:

```
\Microsoft\Windows\DiskCleanup\SilentCleanup
```

Key properties:
- Runs with **highest privileges**
- Callable by **standard users**
- Executes:

```
%windir%\system32\cleanmgr.exe
```

Since `%windir%` is resolved from environment variables, it becomes a controllable primitive.

### Environment Variable Hijacking

ZUAC modifies:

```
HKEY_CURRENT_USER\Environment\WINDIR
```

This allows a non-privileged user to influence how the scheduled task resolves its execution path.

When the task executes:
- `%windir%` is replaced with attacker-controlled data
- The system executes a substituted binary or command chain

This works because privileged processes may inherit or resolve environment variables influenced by user context. :contentReference[oaicite:1]{index=1}

### COM Interaction (Task Scheduler)

The implementation uses:
- `ITaskService`
- `ITaskFolder`
- `IRegisteredTask`

All invoked through **COBJMACROS**, enabling COM interaction in pure C style without C++ abstractions.

Execution flow:
1. Initialize COM (`CoInitializeEx`)
2. Connect to Task Scheduler
3. Retrieve SilentCleanup task
4. Execute via `IRegisteredTask::RunEx`

### Environment Refresh

After modifying the registry, the code broadcasts:

```
WM_SETTINGCHANGE
```

This ensures the updated environment variables are recognized system-wide without requiring a logoff.

### Execution Flow Summary

1. Build payload path
2. Set `WINDIR` in HKCU
3. Broadcast environment update
4. Trigger SilentCleanup task via COM
5. Elevated execution occurs
6. Sleep briefly
7. Remove registry modification (cleanup)

This results in a **fileless elevation primitive**, requiring only transient registry modification.

### OS Behavior Notes

- Behavior depends on Windows build/version
- Some builds patched or hardened this vector
- Quote handling differences are addressed dynamically in code

---

## Compilation

Use **Developer Command Prompt for Visual Studio**:

```
cl /nologo /W4 /DUNICODE /D_UNICODE main.c
```

Required libraries:
- Advapi32.lib
- User32.lib
- Ole32.lib
- OleAut32.lib
- Taskschd.lib

---

## Usage

ZUAC determines the payload as follows:

1. `argv[1]` (highest priority)
2. `lpCmdLine` fallback

Example:

```
zuac.exe C:\Path\to\payload.exe
```

If no argument is provided, it attempts to execute using the raw command line.

---

## Warning

> [!WARNING]  
> This project demonstrates a UAC bypass technique that manipulates user-controlled environment variables and triggers an auto-elevated scheduled task. Execution of this code may be flagged, blocked, or logged by modern security solutions such as EDR, antivirus, or Windows Defender. Behavior may vary across Windows versions due to patches and mitigations.

---

## References

- https://www.tiraniddo.dev/2017/05/exploiting-environment-variables-in.html  
- https://github.com/hfiref0x/UACME  
- https://github.com/blue0x1/uac-bypass  

Additional context:
- SilentCleanup task executes with elevated privileges while referencing `%windir%`, which can be user-controlled via registry.
- Detection rules commonly monitor modification of `HKCU\Environment\windir` due to its association with privilege escalation techniques.

---

## Legal Disclaimer

This project is intended strictly for:
- Security research
- Educational purposes
- Defensive development and testing

Unauthorized use of this code against systems without explicit permission may violate applicable laws and regulations. The author assumes no responsibility for misuse or damage caused by this software.
