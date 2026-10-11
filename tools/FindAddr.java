//Find instructions whose operands mention a VA (any object), by scanning scalar
//operands of the disassembled listing.  The LE stores code refs as (VA-0x10000)
//and data refs as (VA-0x100000), so all three spellings are reported.
//usage: -postScript FindAddr.java 0x722e0 [0x722f8 ...]
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import java.util.*;

public class FindAddr extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Set<Long> want = new HashSet<>();
        for (String a : args) want.add(Long.parseLong(a.replace("0x", ""), 16));
        Listing lst = currentProgram.getListing();
        int hits = 0;
        for (Instruction ins : lst.getInstructions(true)) {
            for (int op = 0; op < ins.getNumOperands(); op++) {
                for (Object o : ins.getOpObjects(op)) {
                    if (!(o instanceof Scalar)) continue;
                    long v = ((Scalar) o).getUnsignedValue();
                    for (long w : want) {
                        if (v == w || v + 0x10000 == w || v + 0x100000 == w ||
                            v == w + 0x10000 || v == w + 0x100000) {
                            println(ins.getAddress() + "  " + ins.toString());
                            hits++;
                            op = 99; break;
                        }
                    }
                }
            }
        }
        println("HITS=" + hits);
    }
}
