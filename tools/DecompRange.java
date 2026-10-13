//Decompile every function whose entry lies in [lo, hi) into <outdir>/f_<entry>.c, and write
//<outdir>/INDEX.txt: entry, size, callers, callees -- one line a function.
//usage: -postScript DecompRange.java <outdir> 0x10000 0x4b000
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.util.TreeSet;

public class DecompRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        long lo = Long.decode(args[1]), hi = Long.decode(args[2]);
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        int n = 0;
        try (PrintWriter idx = new PrintWriter(args[0] + "/INDEX.txt")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(toAddr(lo), true)) {
                long ea = f.getEntryPoint().getOffset();
                if (ea >= hi) break;
                if (f.hasNoReturn()) f.setNoReturn(false);
                DecompileResults r = di.decompileFunction(f, 120, monitor);
                TreeSet<String> in = new TreeSet<>(), out = new TreeSet<>();
                for (Function c : f.getCallingFunctions(monitor)) in.add(c.getEntryPoint().toString());
                for (Function c : f.getCalledFunctions(monitor)) out.add(c.getEntryPoint().toString());
                try (PrintWriter w = new PrintWriter(String.format("%s/f_%05x.c", args[0], ea))) {
                    w.println("// " + f.getName() + " @ " + f.getEntryPoint() + "  size " + f.getBody().getNumAddresses());
                    w.println("// called by: " + in);
                    w.println("// calls:     " + out);
                    w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// FAILED: " + r.getErrorMessage());
                }
                idx.println(String.format("%05x %6d  by %s  calls %s", ea, f.getBody().getNumAddresses(), in, out));
                n++;
            }
        }
        println("RANGE: " + n + " functions written");
    }
}
