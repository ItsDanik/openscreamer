//Find code that references data-object strings, by scanning instruction operands.
//The LE stores data references as offsets, so the stored value is (VA - 0x100000).
//usage: -postScript FindImmRefs.java "Cannot create sprite" "Cannot animate object"
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class FindImmRefs extends GhidraScript {

    static final long DATA0 = 0x100000L, DATA1 = 0x100000L + 0x472C0L;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: FindImmRefs.java <substring> [...]"); return; }

        Memory mem = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();

        // collect target string starts matching any substring
        List<long[]> targets = new ArrayList<>();
        List<String> texts = new ArrayList<>();
        for (String needle : args) {
            byte[] pat = needle.getBytes("ISO-8859-1");
            Address a = toAddr(DATA0);
            while (true) {
                Address f = mem.findBytes(a, pat, null, true, null);
                if (f == null) break;
                long off = f.getOffset();
                if (off >= DATA1) break;
                targets.add(new long[]{off});
                texts.add(needle);
                a = f.add(1);
            }
        }
        println("TARGETS=" + targets.size());

        // scan every instruction operand for the unrelocated form
        Listing lst = currentProgram.getListing();
        InstructionIterator it = lst.getInstructions(true);
        int hits = 0;
        while (it.hasNext()) {
            Instruction ins = it.next();
            for (int o = 0; o < ins.getNumOperands(); o++) {
                for (Object ob : ins.getOpObjects(o)) {
                    if (!(ob instanceof Scalar)) continue;
                    long v = ((Scalar) ob).getUnsignedValue();
                    for (int t = 0; t < targets.size(); t++) {
                        if (v == targets.get(t)[0] - DATA0) {
                            Function f = fm.getFunctionContaining(ins.getAddress());
                            StringBuilder sb = new StringBuilder();
                            for (int k = 0; k < 34; k++) {
                                int c = mem.getByte(toAddr(targets.get(t)[0] + k)) & 0xFF;
                                if (c == 0 || c == 10) break;
                                sb.append(c >= 32 && c < 127 ? (char) c : '.');
                            }
                            println(String.format("  %s  \"%s\"  <- %s  in %s",
                                    ins.getAddress(), sb.toString(), ins.toString(),
                                    f != null ? f.getName() + "@" + f.getEntryPoint()
                                              : "(no func)"));
                            hits++;
                        }
                    }
                }
            }
        }
        println("HITS=" + hits);
    }
}
