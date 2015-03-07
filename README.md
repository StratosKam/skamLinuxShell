# skamLinuxShell
A linux shell implementation for directory changes,executions,pipelines,redirection.
Uses Round Robin to handle background processes.

Tested in Arch.

Usages:

cd "new/dir/" to change directory.

"command"

"command&"

"dir/to/command/command"

"command1 | command2"

"command1 | command2&"

"command1<file || command2>file"

"command>>file"

"command>file"

"command<file"

(spaces can be recognised between the command and the various symbols)
