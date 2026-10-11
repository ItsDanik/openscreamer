//Find instructions reading a NON-stack memory operand at a given displacement --
//i.e. a field of a structure, not a local.  For locating the reader of the face
//record's material field at +0x20.
//usage: -postScript ScanFace.java 0x20
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.*;
import java.util.*;

public class ScanFace extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        long want = Long.parseLong(args[0].replace("0x", ""), 16);
        String pat = "+ 0x" + Long.toHexString(want) + "]";
        long lo = 0x10000L, hi = 0x10000L + 0xDB7A3L;
        AddressSet range = new AddressSet(toAddr(lo), toAddr(hi));
        FunctionManager fm = currentProgram.getFunctionManager();
        Map<String,Integer> byFunc = new LinkedHashMap<>();
        int n = 0;
        for (InstructionIterator it =
                 currentProgram.getListing().getInstructions(range, true); it.hasNext();) {
            Instruction ins = it.next();
            String t = ins.toString();
            if (!t.contains(pat)) continue;
            if (t.contains("[EBP ") || t.contains("[ESP ")) continue;   // stack locals
            Function f = fm.getFunctionContaining(ins.getAddress());
            String key = f != null ? f.getName() + "@" + f.getEntryPoint() : "(no func)";
            byFunc.merge(key, 1, Integer::sum);
            n++;
            if (n <= 30) println("  " + ins.getAddress() + "  " + t + "   in " + key);
        }
        println("TOTAL=" + n);
        byFunc.entrySet().stream().sorted((a,b)->b.getValue()-a.getValue()).limit(15)
              .forEach(e -> println("   " + e.getValue() + "  " + e.getKey()));
    }
}
