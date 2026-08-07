import os
import gdb


_CONTROLLER_ATTR = "_autark_until_bp_controller"
_COMMAND_ATTR = "_autark_until_bp_command"

_SHELL_NAMES = {
    "sh",
    "dash",
    "bash",
    "ash",
    "zsh",
    "ksh",
    "fish",
}


def _get_event_breakpoints(event):
    """Return breakpoints associated with a BreakpointEvent."""

    breakpoints = getattr(event, "breakpoints", None)

    if breakpoints is not None:
        return tuple(breakpoints)

    # Compatibility with older GDB versions.
    breakpoint = getattr(event, "breakpoint", None)

    if breakpoint is not None:
        return (breakpoint,)

    return ()


class UntilBreakpointController:
    def __init__(self):
        self.active = False
        self.resume_pending = False

        gdb.events.stop.connect(self.on_stop)
        gdb.events.exited.connect(self.on_inferior_exit)

    def uninstall(self):
        self.active = False
        self.resume_pending = False

        try:
            gdb.events.stop.disconnect(self.on_stop)
        except Exception:
            pass

        try:
            gdb.events.exited.disconnect(self.on_inferior_exit)
        except Exception:
            pass

    @staticmethod
    def is_live(inferior):
        try:
            return inferior.is_valid() and bool(inferior.threads())
        except (gdb.error, RuntimeError):
            return False

    @staticmethod
    def inferior_filename(inferior):
        try:
            progspace = inferior.progspace

            return (
                getattr(progspace, "executable_filename", None)
                or getattr(progspace, "filename", None)
                or ""
            )
        except (gdb.error, RuntimeError):
            return ""

    def select_live_inferior(self):
        """
        Select a live inferior.

        Prefer the currently selected inferior. If it has exited, prefer
        a shell, because it usually launches the next command.
        """

        try:
            current = gdb.selected_inferior()
        except gdb.error:
            current = None

        if current is not None and self.is_live(current):
            return current

        live = [
            inferior
            for inferior in gdb.inferiors()
            if self.is_live(inferior)
        ]

        if not live:
            return None

        shells = [
            inferior
            for inferior in live
            if os.path.basename(
                self.inferior_filename(inferior)
            ) in _SHELL_NAMES
        ]

        target = max(
            shells or live,
            key=lambda inferior: inferior.num,
        )

        gdb.execute(
            f"inferior {target.num}",
            from_tty=False,
            to_string=True,
        )

        return target

    def stop_mode(self, message=None):
        self.active = False
        self.resume_pending = False

        if message:
            gdb.write(f"until_bp: {message}\n")

    def schedule_continue(self):
        if not self.active or self.resume_pending:
            return

        self.resume_pending = True
        gdb.post_event(self.continue_execution)

    def continue_execution(self):
        self.resume_pending = False

        if not self.active:
            return

        inferior = self.select_live_inferior()

        if inferior is None:
            self.stop_mode(
                "all inferiors exited before reaching a breakpoint"
            )
            return

        try:
            gdb.execute(
                "continue",
                from_tty=False,
                to_string=False,
            )
        except gdb.error as exc:
            self.stop_mode(f"continue failed: {exc}")

    def start(self):
        if self.active:
            gdb.write("until_bp: already active\n")
            return

        inferior = self.select_live_inferior()

        if inferior is None:
            raise gdb.GdbError(
                "No live inferior. Run the program first."
            )

        self.active = True
        self.resume_pending = False

        gdb.write(
            "until_bp: continuing until a breakpoint is reached\n"
        )

        # Direct invocation is safe here: start() is called from a user
        # command, not from an event handler.
        try:
            gdb.execute(
                "continue",
                from_tty=False,
                to_string=False,
            )
        except gdb.error as exc:
            self.stop_mode()
            raise gdb.GdbError(str(exc))

    def on_stop(self, event):
        if not self.active:
            return

        if isinstance(event, gdb.BreakpointEvent):
            breakpoints = _get_event_breakpoints(event)

            if not breakpoints:
                self.stop_mode(
                    "unclassified breakpoint event; automatic continue disabled"
                )
                return

            catchpoint_type = getattr(
                gdb,
                "BP_CATCHPOINT",
                None,
            )

            only_catchpoints = True

            for breakpoint in breakpoints:
                try:
                    if breakpoint.type != catchpoint_type:
                        only_catchpoints = False
                        break
                except (gdb.error, RuntimeError):
                    # A temporary/deleted breakpoint should be treated
                    # as a real stopping point.
                    only_catchpoints = False
                    break

            if only_catchpoints:
                # For example: catch exec.
                self.schedule_continue()
                return

            numbers = []

            for breakpoint in breakpoints:
                try:
                    numbers.append(str(breakpoint.number))
                except (gdb.error, RuntimeError):
                    pass

            suffix = (
                f" {', '.join(numbers)}"
                if numbers
                else ""
            )

            self.stop_mode(
                f"breakpoint{suffix} reached"
            )
            return

        if isinstance(event, gdb.SignalEvent):
            signal_name = getattr(
                event,
                "stop_signal",
                "unknown signal",
            )

            self.stop_mode(
                f"stopped by {signal_name}"
            )
            return

        details = getattr(event, "details", {})
        reason = details.get("reason", "unknown stop reason")

        # Do not automatically continue unknown events. This also makes
        # Ctrl-C and unexpected debugger stops safe.
        self.stop_mode(
            f"stopped: {reason}"
        )

    def on_inferior_exit(self, event):
        if not self.active:
            return

        # Delay selection until GDB has completely processed the exit.
        self.schedule_continue()


class UntilBreakpointCommand(gdb.Command):
    """
    Continue across exec catchpoints and child-process exits until a real
    breakpoint or watchpoint is reached.

    Usage:
        until_bp
        until_bp off
        until_bp status
    """

    def __init__(self, controller):
        super().__init__(
            "until_bp",
            gdb.COMMAND_RUNNING,
        )

        self.controller = controller

    def invoke(self, argument, from_tty):
        self.dont_repeat()

        arguments = gdb.string_to_argv(argument)

        if not arguments:
            self.controller.start()
            return

        if arguments == ["off"]:
            self.controller.stop_mode(
                "automatic continue disabled"
            )
            return

        if arguments == ["status"]:
            status = (
                "active"
                if self.controller.active
                else "inactive"
            )

            gdb.write(f"until_bp: {status}\n")
            return

        raise gdb.GdbError(
            "Usage: until_bp [off|status]"
        )


# Reload support: disconnect handlers from the previously loaded controller.
old_controller = getattr(
    gdb,
    _CONTROLLER_ATTR,
    None,
)

if old_controller is not None:
    old_controller.uninstall()

controller = UntilBreakpointController()

# A Python GDB command cannot be unregistered. Therefore, when the script is
# sourced again, reuse the existing command and replace only its controller.
command = getattr(
    gdb,
    _COMMAND_ATTR,
    None,
)

if command is None:
    command = UntilBreakpointCommand(controller)
    setattr(gdb, _COMMAND_ATTR, command)
else:
    command.controller = controller

setattr(gdb, _CONTROLLER_ATTR, controller)

gdb.write("auto-inferior hook loaded; command: until_bp\n")