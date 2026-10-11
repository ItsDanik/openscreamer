//Decompile a function by address.  usage: -postScript DecompFunc.java 0x10bfc
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompFunc extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: DecompFunc.java <hexaddr> [hexaddr...]"); return; }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (String arg : args) {
            long va = Long.parseLong(arg.replace("0x", ""), 16);
            Address ea = toAddr(va);

            FunctionManager fm = currentProgram.getFunctionManager();
            Function f = fm.getFunctionAt(ea);
            if (f == null) f = fm.getFunctionContaining(ea);
            if (f == null) { println("no function at " + ea); continue; }

            println("FUNCTION " + f.getName() + " @ " + f.getEntryPoint()
                    + "  size=" + f.getBody().getNumAddresses());
            DecompileResults res = di.decompileFunction(f, 240, new ConsoleTaskMonitor());
            if (res.decompileCompleted()) {
                println("=====DECOMP " + f.getEntryPoint() + "=====");
                println(res.getDecompiledFunction().getC());
            } else {
                println("decompile failed for " + ea + ": " + res.getErrorMessage());
            }
        }
    }
}
