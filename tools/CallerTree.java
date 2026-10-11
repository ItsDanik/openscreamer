//Decompile a function AND every function that references it, so the data layout
//can be read from both the producer and the consumers.
//For MMP: the relocator only proves a field is a pointer; the consumer proves
//what it means.
//usage: -postScript CallerTree.java 0x4c904
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class CallerTree extends GhidraScript {

    void dec(DecompInterface di, Function f) {
        DecompileResults r = di.decompileFunction(f, 60, monitor);
        println("\n==== " + f.getName() + " @ " + f.getEntryPoint() + " ====");
        if (r == null || !r.decompileCompleted()) {
            println("   (decompile failed)");
            return;
        }
        println(r.getDecompiledFunction().getC());
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: CallerTree.java <addr>"); return; }
        Address target = toAddr(args[0]);

        FunctionManager fm = currentProgram.getFunctionManager();
        Function tf = fm.getFunctionAt(target);
        if (tf == null) tf = fm.getFunctionContaining(target);
        println("TARGET: " + target + " -> " + (tf == null ? "(not in a function)" : tf.getName()));

        // who references the target?
        Set<Function> callers = new LinkedHashSet<>();
        ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(target);
        while (ri.hasNext()) {
            Reference ref = ri.next();
            Address from = ref.getFromAddress();
            Function f = fm.getFunctionContaining(from);
            println("  ref from " + from + " type=" + ref.getReferenceType()
                    + " in " + (f == null ? "(no func)" : f.getName()));
            if (f != null && (tf == null || !f.getEntryPoint().equals(tf.getEntryPoint())))
                callers.add(f);
        }
        println("CALLERS=" + callers.size());

        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);

        if (tf != null) dec(di, tf);
        for (Function f : callers) dec(di, f);
    }
}
