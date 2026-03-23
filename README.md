# ZUAC (Zeppelin UAC)

## Overview

**ZUAC** is a pure C implementation targeting Windows environments, demonstrating a privilege escalation technique via the built-in **SilentCleanup** scheduled task.

The method leverages user-controlled environment variable manipulation to influence task execution context. Specifically, it hijacks the `WINDIR` environment variable within the current user registry hive, causing the task to execute an arbitrary payload with elevated privileges.

This implementation avoids C++ abstractions and relies entirely on native Win32 and COM interfaces, using `COBJMACROS` for C-style interaction with the Task Scheduler API.

Maintained by **@undefinedable**.

---

## Technical Details

### SilentCleanup Task Interaction

The escalation mechanism targets the scheduled task:

```
\Microsoft\Windows\DiskCleanup\SilentCleanup
```

Execution is performed via COM interfaces:

* `ITaskService`
* `ITaskFolder`
* `IRegisteredTask`
* `IRunningTask`

Using `COBJMACROS`, the implementation interacts with COM objects in pure C style:

```c
ITaskService_Connect(...)
ITaskFolder_GetTask(...)
IRegisteredTask_RunEx(...)
```

---

### Environment Variable Hijacking

The core primitive relies on modifying:

```
HKCU\Environment\WINDIR
```

Steps:

1. Set a user-controlled `WINDIR` value pointing to the payload
2. Broadcast environment changes system-wide via `WM_SETTINGCHANGE`
3. Trigger the SilentCleanup task
4. The task resolves `%WINDIR%` from the user context, executing the injected payload

---

### OS Version Handling

The implementation queries the OS version via:

```c
RtlGetVersion (ntdll.dll)
```

* Applies conditional quoting logic for Windows 10 builds ≥ 19044
* Ensures compatibility with argument parsing behavior changes

---

### Registry Cleanup

After task execution:

* The `WINDIR` value is removed from:

  ```
  HKCU\Environment
  ```
* Environment is restored to its original state

---

## Compilation

Use **Developer Command Prompt for Visual Studio**:

```
cl /nologo /W4 /DUNICODE /D_UNICODE main.c
```

Required libraries:

* `Advapi32.lib`
* `User32.lib`
* `Ole32.lib`
* `OleAut32.lib`
* `Taskschd.lib`

---

## Usage

```
ZUAC.exe [payload_path]
```

* If `argv[1]` is provided, it is used as the payload path
* Otherwise, falls back to `lpCmdLine`

Execution flow:

1. Set `WINDIR` environment variable (user scope)
2. Broadcast environment update
3. Invoke SilentCleanup scheduled task
4. Execute payload under elevated context
5. Remove modified registry value

---

## Notes

> [!IMPORTANT]
> The `WM_SETTINGCHANGE` broadcast is required to propagate the modified environment variable before triggering the scheduled task.

> [!WARNING]
> Interaction with scheduled tasks and environment manipulation may be monitored or blocked by endpoint protection systems.

---

## Legal Disclaimer

> [!WARNING]
> This project is intended strictly for educational purposes, security research, and authorized testing. Unauthorized use against systems without explicit permission may violate applicable laws and regulations.

> [!CAUTION]
> Execution in non-isolated environments may trigger defensive controls or result in unintended system behavior.
