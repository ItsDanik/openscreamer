//Report functions referencing given data-VAs. usage: -postScript RefsToVA.java 0x1004f1 0x1009c9
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import java.util.*;

public class RefsToVA extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        for (String arg : getScriptArgs()) {
            long va = Long.parseLong(arg.replace("0x", ""), 16);
            Address a = toAddr(va);
            StringBuilder sb = new StringBuilder();
            for (int k = 0; k < 40; k++) {
                int c = mem.getByte(a.add(k)) & 0xFF;
                if (c == 0 || c == 10) break;
                sb.append(c >= 32 && c < 127 ? (char) c : '.');
            }
            Set<String> funcs = new TreeSet<>();
            Set<String> sites = new TreeSet<>();
            for (Reference r : rm.getReferencesTo(a)) {
                Function f = fm.getFunctionContaining(r.getFromAddress());
                funcs.add(f != null ? f.getName() + "@" + f.getEntryPoint()
                                    : "(no func)");
                sites.add(r.getFromAddress().toString());
            }
            println(String.format("0x%06X %-30s sites=%s", va, sb.toString(), sites));
            println(String.format("         functions: %s", funcs));
        }
    }
}
