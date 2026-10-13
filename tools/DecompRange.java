//Decompile every function whose entry lies in [lo, hi) into <outdir>/f_<entry>.c, and write
//<outdir>/INDEX.txt: entry, size, callers, callees -- one line a function.
//usage: -postScript DecompRange.java <outdir> 0x10000 0x4b000
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.data.Pointer;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.symbol.SourceType;
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
                /* a pointer parameter makes the decompiler write every field as `p[i]` with
                   casts that do not say whether a float is READ or an int CONVERTED; as a plain
                   int the fields come out as *(float *)(p + off) / *(int *)(p + off) */
                try {
                    if (r.decompileCompleted() && r.getHighFunction() != null) {
                        HighFunctionDBUtil.commitParamsToDatabase(r.getHighFunction(), true, HighFunctionDBUtil.ReturnCommitOption.NO_COMMIT, SourceType.USER_DEFINED);
                        boolean changed = false;
                        for (Parameter q : f.getParameters())
                            if (q.getDataType() instanceof Pointer) { q.setDataType(IntegerDataType.dataType, SourceType.USER_DEFINED); changed = true; }
                        if (changed) r = di.decompileFunction(f, 120, monitor);
                        /* the same for LOCALS the decompiler makes pointers (a saved copy of
                           the record's address): as ints their fields are spelled out too */
                        boolean again = false;
                        for (int pass = 0; pass < 3 && r.decompileCompleted(); pass++) {
                            boolean any = false;
                            java.util.Iterator<ghidra.program.model.pcode.HighSymbol> ls = r.getHighFunction().getLocalSymbolMap().getSymbols();
                            while (ls.hasNext()) {
                                ghidra.program.model.pcode.HighSymbol hs = ls.next();
                                if (hs.isParameter() || !(hs.getDataType() instanceof Pointer)) continue;
                                if (hs.getDataType().getLength() != 4) continue;
                                try {
                                    HighFunctionDBUtil.updateDBVariable(hs, null, IntegerDataType.dataType, SourceType.USER_DEFINED);
                                    any = true;
                                } catch (Exception e) { }
                            }
                            if (!any) break;
                            r = di.decompileFunction(f, 120, monitor);
                        }
                    }
                        /* and GLOBALS that hold an address: defined as an int in the listing */
                        if (r.decompileCompleted() && r.getHighFunction() != null) {
                            boolean g = false;
                            java.util.Iterator<ghidra.program.model.pcode.HighSymbol> gs = r.getHighFunction().getGlobalSymbolMap().getSymbols();
                            while (gs.hasNext()) {
                                ghidra.program.model.pcode.HighSymbol hs = gs.next();
                                if (!(hs.getDataType() instanceof Pointer) || hs.getSize() != 4) continue;
                                if (hs.getStorage() == null || !hs.getStorage().isMemoryStorage()) continue;
                                ghidra.program.model.address.Address ga = hs.getStorage().getMinAddress();
                                if (ga.getOffset() < 0x119000L) continue;        /* (not the initialised tables) */
                                try {
                                    currentProgram.getListing().clearCodeUnits(ga, ga.add(3), false);
                                    currentProgram.getListing().createData(ga, IntegerDataType.dataType);
                                    g = true;
                                } catch (Exception e) { }
                            }
                            if (g) r = di.decompileFunction(f, 120, monitor);
                        }
                } catch (Exception e) { println("PARAMS: " + f.getEntryPoint() + " " + e.getMessage()); }
                TreeSet<String> in = new TreeSet<>(), out = new TreeSet<>();
                for (Function c : f.getCallingFunctions(monitor)) in.add(c.getEntryPoint().toString());
                for (Function c : f.getCalledFunctions(monitor)) out.add(c.getEntryPoint().toString());
                try (PrintWriter w = new PrintWriter(String.format("%s/f_%05x.c", args[0], ea))) {
                    w.println("// " + f.getName() + " @ " + f.getEntryPoint() + "  size " + f.getBody().getNumAddresses());
                    w.println("// called by: " + in);
                    w.println("// calls:     " + out);
                    /* the globals it uses, as the decompiler types them (the text does not say) */
                    if (r.decompileCompleted() && r.getHighFunction() != null) {
                        java.util.Iterator<ghidra.program.model.pcode.HighSymbol> it = r.getHighFunction().getGlobalSymbolMap().getSymbols();
                        while (it.hasNext()) {
                            ghidra.program.model.pcode.HighSymbol hs = it.next();
                            if (hs.getStorage() == null || !hs.getStorage().isMemoryStorage()) continue;
                            w.println("// GLOBAL " + hs.getName() + " @" + hs.getStorage().getMinAddress() + " " + hs.getSize() + " " + hs.getDataType().getName().replace(' ', '_'));
                        }
                    }
                    w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// FAILED: " + r.getErrorMessage());
                }
                idx.println(String.format("%05x %6d  by %s  calls %s", ea, f.getBody().getNumAddresses(), in, out));
                n++;
            }
        }
        println("RANGE: " + n + " functions written");
    }
}
