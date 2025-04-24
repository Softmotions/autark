cd ./tests
file ./test4
set args -V

set confirm off
set follow-fork-mode parent
set detach-on-fork on
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