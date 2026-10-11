//Find every function that reads a global, decompile each, and print the bytes
//at a data address.  For the MMP: DAT_00002fa0 is the file base, so its readers
//are exactly the code that consumes the placement grid.
//usage: -postScript GlobalUsers.java 0x2fa0 [--str 0x7fb0]
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.*;
import java.util.*;

public class GlobalUsers extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: GlobalUsers.java <addr> [--str addr]"); return; }

        Memory mem = currentProgram.getMemory();
        if (args.length >= 3 && args[1].equals("--str")) {
            Address s = toAddr(args[2]);
            StringBuilder sb = new StringBuilder();
            for (int k = 0; k < 120; k++) {
                int c = mem.getByte(s.add(k)) & 0xFF;
                if (c == 0) break;
                sb.append(c >= 32 && c < 127 ? (char) c : '.');
            }
            println("STRING @ " + s + " = \"" + sb + "\"");
        }

        Address target = toAddr(args[0]);
        FunctionManager fm = currentProgram.getFunctionManager();
        Set<Function> users = new LinkedHashSet<>();
        ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(target);
        while (ri.hasNext()) {
            Reference ref = ri.next();
            Function f = fm.getFunctionContaining(ref.getFromAddress());
            println("  read from " + ref.getFromAddress() + " in "
                    + (f == null ? "(no func)" : f.getName()));
            if (f != null) users.add(f);
        }
        println("USERS=" + users.size());

        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (Function f : users) {
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            println("\n==== " + f.getName() + " @ " + f.getEntryPoint() + " ====");
            println(r != null && r.decompileCompleted()
                    ? r.getDecompiledFunction().getC() : "(failed)");
        }
    }
}
