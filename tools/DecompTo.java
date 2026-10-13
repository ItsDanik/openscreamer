//Decompile the functions holding the given addresses into files, one per function.
//usage: -postScript DecompTo.java <outdir> 0x4b547 0x4f78e ...
//An address inside a function selects that function; an address in no function becomes a
//new function's entry.  Each file starts with the function's callers and callees.
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.util.TreeSet;

public class DecompTo extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (int i = 1; i < args.length; i++) {
            Address ea = toAddr(Long.decode(args[i]));
            Function f = currentProgram.getFunctionManager().getFunctionContaining(ea);
            if (f == null) {
                new DisassembleCommand(ea, null, true).applyTo(currentProgram, monitor);
                new CreateFunctionCmd(ea).applyTo(currentProgram, monitor);
                f = currentProgram.getFunctionManager().getFunctionAt(ea);
            }
            if (f == null) { println("DECOMP: no function at " + ea); continue; }
            if (f.hasNoReturn()) f.setNoReturn(false);
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            String name = String.format("%s/f_%05x.c", args[0], f.getEntryPoint().getOffset());
            try (PrintWriter w = new PrintWriter(name)) {
                TreeSet<String> in = new TreeSet<>(), out = new TreeSet<>();
                for (Function c : f.getCallingFunctions(monitor)) in.add(c.getEntryPoint().toString());
                for (Function c : f.getCalledFunctions(monitor)) out.add(c.getEntryPoint().toString());
                w.println("// " + f.getName() + " @ " + f.getEntryPoint() + "  size " + f.getBody().getNumAddresses());
                w.println("// called by: " + in);
                w.println("// calls:     " + out);
                w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// FAILED: " + r.getErrorMessage());
            }
            println("DECOMP: " + args[i] + " -> " + name + " (" + f.getBody().getNumAddresses() + " bytes)");
        }
    }
}
