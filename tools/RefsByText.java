//Report which functions reference strings containing any of the given substrings.
//usage: -postScript RefsByText.java graf.dat gfx\ data\
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import java.util.*;

public class RefsByText extends GhidraScript {

    static final long DATA0 = 0x100000L, DATA1 = 0x100000L + 0x472C0L;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: RefsByText.java <substring> [...]"); return; }

        Memory mem = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();

        // collect every string start in the data object, then filter by substring
        List<long[]> hits = new ArrayList<>();
        List<String> texts = new ArrayList<>();
        for (String needle : args) {
            byte[] pat = needle.getBytes("ISO-8859-1");
            Address a = mem.getMinAddress();
            while (true) {
                Address found = mem.findBytes(a, pat, null, true, null);
                if (found == null) break;
                long off = found.getOffset();
                if (off >= DATA0 && off < DATA1) {
                    hits.add(new long[]{off});
                    texts.add(needle);
                }
                a = found.add(1);
                if (off >= DATA1) break;
            }
        }
        println("matched " + hits.size() + " occurrence(s)");

        for (int i = 0; i < hits.size(); i++) {
            long va = hits.get(i)[0];
            Address sa = toAddr(va);
            // read the string text for display
            StringBuilder sb = new StringBuilder();
            for (int k = 0; k < 48; k++) {
                int c = mem.getByte(sa.add(k)) & 0xFF;
                if (c == 0 || c == 10) break;
                sb.append(c >= 32 && c < 127 ? (char) c : '.');
            }
            Set<String> funcs = new TreeSet<>();
            for (Reference r : rm.getReferencesTo(sa)) {
                Function f = fm.getFunctionContaining(r.getFromAddress());
                funcs.add(f != null ? f.getName() + "@" + f.getEntryPoint()
                                    : r.getFromAddress().toString());
            }
            println(String.format("  0x%06X  %-34s  <- %s", va, sb.toString(),
                    funcs.isEmpty() ? "(no refs yet)" : funcs.toString()));
        }
    }
}
