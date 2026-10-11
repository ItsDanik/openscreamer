//Settle the LE relocation question: which transform makes instruction operands
//land on real string addresses in the data object? Uses REAL instruction operands
//(not byte patterns).
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class TestReloc extends GhidraScript {

    static final long DATA0 = 0x100000L;
    static final long DATA1 = 0x100000L + 0x472C0L;
    static final long CODE0 = 0x10000L;

    private final Set<Long> strings = new HashSet<>();
    private final Map<String, Integer> hits = new LinkedHashMap<>();
    private final Map<String, String> example = new HashMap<>();
    private final Map<String, Map<Integer, Integer>> widthHist = new LinkedHashMap<>();

    private static final String[] NAMES = {
        "abs", "+codebase", "+database", "rel_insn", "rel_insn+1",
        "rel_field", "rel_field+1"
    };

    private void note(String name, long target, long insnAddr, int bits, long raw) {
        if (!strings.contains(target)) return;
        hits.merge(name, 1, Integer::sum);
        widthHist.computeIfAbsent(name, k -> new TreeMap<>())
                 .merge(bits, 1, Integer::sum);
        example.putIfAbsent(name, String.format(
            "e.g. insn %x value %x -> %x", insnAddr, raw, target));
    }

    /** offset of the 4-byte LE encoding of v inside the instruction, or -1 */
    private long fieldAddr(Instruction ins, long v) {
        try {
            int len = ins.getLength();
            byte[] b = new byte[len];
            currentProgram.getMemory().getBytes(ins.getAddress(), b);
            long base = ins.getAddress().getOffset();
            long want = v & 0xFFFFFFFFL;
            for (int k = 0; k + 4 <= len; k++) {
                long got = (b[k] & 0xFFL) | ((b[k+1] & 0xFFL) << 8)
                         | ((b[k+2] & 0xFFL) << 16) | ((b[k+3] & 0xFFL) << 24);
                if (got == want) return base + k;
            }
        } catch (Exception e) { /* ignore */ }
        return -1;
    }

    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        for (String n : NAMES) hits.put(n, 0);

        // ---- 1. every string start inside the data object ----
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
        println("STRING_STARTS=" + strings.size());

        // ---- 2. walk REAL instructions, test transforms on REAL operands ----
        Listing lst = currentProgram.getListing();
        InstructionIterator it = lst.getInstructions(true);
        long nOps = 0, nInsn = 0;
        while (it.hasNext()) {
            Instruction ins = it.next();
            nInsn++;
            long ia = ins.getAddress().getOffset();
            for (int o = 0; o < ins.getNumOperands(); o++) {
                for (Object ob : ins.getOpObjects(o)) {
                    if (!(ob instanceof Scalar)) continue;
                    Scalar s = (Scalar) ob;
                    if (s.bitLength() > 32) continue;
                    long v = s.getUnsignedValue();
                    int bits = s.bitLength();
                    nOps++;
                    long f = fieldAddr(ins, v);
                    note("abs",          v,             ia, bits, v);
                    note("+codebase",    v + CODE0,     ia, bits, v);
                    note("+database",    v + DATA0,     ia, bits, v);
                    note("rel_insn",     ia + v,        ia, bits, v);
                    note("rel_insn+1",   ia + 1 + v,    ia, bits, v);
                    if (f >= 0) {
                        note("rel_field",   f + v,      ia, bits, v);
                        note("rel_field+1", f + 1 + v,  ia, bits, v);
                    }
                }
            }
        }
        println("INSTRUCTIONS_WALKED=" + nInsn + "  SCALAR_OPERANDS=" + nOps);
        println("--- hits on real string addresses, per candidate transform ---");
        for (String n : NAMES) {
            println(String.format("  %-12s hits=%-6d %s", n, hits.get(n),
                    widthHist.getOrDefault(n, new TreeMap<>())));
        }
        println("--- examples ---");
        for (String n : NAMES) {
            if (hits.get(n) > 0) println("  " + n + ": " + example.get(n));
        }
    }
}
