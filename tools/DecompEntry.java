//Decompile the Watcom CRT entry point of the Screamer flat image
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompEntry extends GhidraScript {
    @Override
    public void run() throws Exception {
        long entryVa = 0x2FEB8L;   // Watcom CRT startup (obj1 base 0x10000 + 0x1FEB8)
        Address ea = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(entryVa);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();

        new DisassembleCommand(ea, null, true).applyTo(currentProgram, mon);

        FunctionManager fm = currentProgram.getFunctionManager();
        Function func = fm.getFunctionAt(ea);
        if (func == null) {
            new CreateFunctionCmd(ea).applyTo(currentProgram, mon);
            func = fm.getFunctionAt(ea);
        }
        println("FUNC  = " + func);
        if (func == null) { println("could not create function"); return; }

        Listing listing = currentProgram.getListing();
        InstructionIterator it = listing.getInstructions(func.getBody(), true);
        int n = 0;
        println("--- first instructions ---");
        while (it.hasNext() && n < 20) {
            Instruction ins = it.next();
            println("   " + ins.getAddress() + "   " + ins.toString());
            n++;
        }

        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        DecompileResults res = di.decompileFunction(func, 180, mon);
        println("=====DECOMP=====");
        if (res.decompileCompleted()) {
            println(res.getDecompiledFunction().getC());
        } else {
            println("decompile failed: " + res.getErrorMessage());
        }
    }
}
