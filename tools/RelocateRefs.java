//Create references for object-relative operands: the LE stores data-object
//references as offsets, relocated by adding the object base 0x100000.
//Only creates a reference when the target is an exact string start or a known
//function entry, so it cannot corrupt anything.
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;
import java.util.*;

public class RelocateRefs extends GhidraScript {

    static final long DATA0 = 0x100000L, DATA1 = 0x100000L + 0x472C0L;
    static final long CODE0 = 0x10000L;
    static final long CODE1 = 0x10000L + 0xDB7A3L;

    private final Set<Long> strings = new HashSet<>();

    private void loadStrings(Memory mem) throws Exception {
        int size = (int) (DATA1 - DATA0);
        byte[] buf = new byte[size];
        mem.getBytes(toAddr(DATA0), buf);
        int i = 0;
        while (i < buf.length) {
            boolean prevOk = (i == 0) || buf[i-1] == 0 || buf[i-1] == 10;
            if (prevOk && buf[i] >= 32 && buf[i] < 127) {
                int j = i;
                while (j < buf.length && buf[j] != 0
                       && ((buf[j] >= 32 && buf[j] < 127) || buf[j] == 9 || buf[j] == 10)) j++;
                if (j - i >= 4) { strings.add(DATA0 + i); i = j; continue; }
            }
            i++;
        }
    }

    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        loadStrings(mem);

        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();
        Listing lst = currentProgram.getListing();

        Set<Long> funcEntries = new HashSet<>();
        for (Function f : fm.getFunctions(true)) funcEntries.add(f.getEntryPoint().getOffset());

        int nData = 0, nCode = 0;
        InstructionIterator it = lst.getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            for (int o = 0; o < ins.getNumOperands(); o++) {
                for (Object ob : ins.getOpObjects(o)) {
                    if (!(ob instanceof Scalar)) continue;
                    Scalar s = (Scalar) ob;
                    if (s.bitLength() != 32) continue;      // only true 32-bit immediates
                    long v = s.getUnsignedValue();
                    long dt = v + DATA0;
                    long ct = v + CODE0;
                    if (dt > DATA0 && dt < DATA1 && strings.contains(dt)) {
                        rm.addMemoryReference(ins.getAddress(), toAddr(dt),
                                RefType.DATA, SourceType.USER_DEFINED, o);
                        nData++;
                    } else if (ct > CODE0 && ct < CODE1 && funcEntries.contains(ct)) {
                        rm.addMemoryReference(ins.getAddress(), toAddr(ct),
                                RefType.DATA, SourceType.USER_DEFINED, o);
                        nCode++;
                    }
                }
            }
        }
        println("REFERENCES_CREATED data=" + nData + " code=" + nCode);

        // which functions reference the asset-loading strings now?
        String[] keys = {"%s.3DX", "3D_HEADER", "3D_BLKOBJ",
                         "Error while searching for object file",
                         "An error occurred while opening file",
                         "File %s is corrupted.", "Out of file domain.",
                         "File %s not present in main file.", "screamer.cfg"};
        for (String k : keys) {
            byte[] pat = k.getBytes("ISO-8859-1");
            Address hit = mem.findBytes(mem.getMinAddress(), pat, null, true, null);
            if (hit == null) { println("  [missing] " + k); continue; }
            StringBuilder sb = new StringBuilder();
            for (Reference r : rm.getReferencesTo(hit)) {
                Function f = fm.getFunctionContaining(r.getFromAddress());
                sb.append(" ").append(f != null ? f.getName() + "@" + f.getEntryPoint()
                                                : r.getFromAddress().toString());
            }
            println("  " + k + "  <-" + sb);
        }
    }
}
