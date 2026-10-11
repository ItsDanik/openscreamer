//Disassemble the entire code object so operand scans cover everything.
//Analysis from the CRT entry point only reaches ~20% of the code; the rest is
//reached through function pointers and never gets discovered.
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.AddressSet;
import ghidra.util.task.ConsoleTaskMonitor;

public class DisasmAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        long lo = 0x10000L, hi = 0x10000L + 0xDB7A3L;   /* obj1, the code object */
        AddressSet range = new AddressSet(toAddr(lo), toAddr(hi));
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        new DisassembleCommand(range, range, true).applyTo(currentProgram, mon);

        long n = 0;
        for (var it = currentProgram.getListing().getInstructions(range, true);
             it.hasNext(); it.next()) n++;
        println("DISASM: " + n + " instructions in the code object");
    }
}
