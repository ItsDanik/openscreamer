//Scan every instruction in the code object for a memory operand with a given
//displacement, optionally restricted to word-sized reads.  For finding the code
//that reads face record fields like the material at +0x1e.
//usage: -postScript ScanDisp.java 0x1e word
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class ScanDisp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: ScanDisp.java <disp> [word|dword]"); return; }
        long want = Long.parseLong(args[0].replace("0x", ""), 16);
        String size = args.length > 1 ? args[1] : null;

        long lo = 0x10000L, hi = 0x10000L + 0xDB7A3L;
        AddressSet range = new AddressSet(toAddr(lo), toAddr(hi));
        Listing lst = currentProgram.getListing();
        FunctionManager fm = currentProgram.getFunctionManager();

        Map<String,Integer> byFunc = new LinkedHashMap<>();
        int n = 0;
        for (InstructionIterator it = lst.getInstructions(range, true); it.hasNext();) {
            Instruction ins = it.next();
            String t = ins.toString();
            if (size != null && t.indexOf(size + " ptr") < 0) continue;
            boolean hit = false;
            for (int o = 0; o < ins.getNumOperands(); o++) {
                for (Object ob : ins.getOpObjects(o)) {
                    if (ob instanceof Scalar
                            && ((Scalar) ob).getUnsignedValue() == want) hit = true;
                }
                if (t.contains("+ 0x" + Long.toHexString(want) + "]")) hit = true;
            }
            if (!hit) continue;
            Function f = fm.getFunctionContaining(ins.getAddress());
            String key = f != null ? f.getName() + "@" + f.getEntryPoint() : "(no func)";
            byFunc.merge(key, 1, Integer::sum);
            n++;
            if (n <= 40)
                println(String.format("  %s  %-40s in %s", ins.getAddress(), t, key));
        }
        println("TOTAL=" + n);
        println("by function:");
        byFunc.entrySet().stream()
              .sorted((a,b) -> b.getValue() - a.getValue())
              .limit(25)
              .forEach(e -> println("   " + e.getValue() + "  " + e.getKey()));
    }
}
