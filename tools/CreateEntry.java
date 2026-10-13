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
        // entry address as the script's argument (Screamer 2: 0xB3D10); default Screamer 1's
        String[] args = getScriptArgs();
        long entry = args.length > 0 ? Long.decode(args[0]) : 0x2FEB8L;
        Address ea = currentProgram.getAddressFactory()
                .getDefaultAddressSpace().getAddress(entry);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        new DisassembleCommand(ea, null, true).applyTo(currentProgram, mon);
        if (currentProgram.getFunctionManager().getFunctionAt(ea) == null) {
            new CreateFunctionCmd(ea).applyTo(currentProgram, mon);
        }
        println("SEED: function created at " + ea);
    }
}
