//Create a function at the Watcom CRT entry point so auto-analysis has a seed
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.util.task.ConsoleTaskMonitor;

public class CreateEntry extends GhidraScript {
    @Override
    public void run() throws Exception {
        Address ea = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(0x2FEB8L);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        new DisassembleCommand(ea, null, true).applyTo(currentProgram, mon);
        if (currentProgram.getFunctionManager().getFunctionAt(ea) == null) {
            new CreateFunctionCmd(ea).applyTo(currentProgram, mon);
        }
        println("SEED: function created at " + ea);
    }
}
