source ./.gdb/auto-inferior.py

cd ./autark-cache/tests
file ./test11

#file ./autark-cache/autark
#set args --compile-commands /home/adam/Projects/softmotions/iowow

#set detach-on-fork off
#catch exec

set follow-fork-mode parent
set schedule-multiple on
set breakpoint pending on

set confirm off
set print elements 4096

handle SIGUSR1 pass nostop print

define lb
    set breakpoint pending on
    source ~/.breakpoints-autark
    set breakpoint pending auto
    echo breakpoints loaded\n
end

define sb
    save breakpoints ~/.breakpoints-autark
    echo breakpoints saved\n
end