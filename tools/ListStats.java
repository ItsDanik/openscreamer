//Summarise analysis results and find code that references key game strings
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.util.DefinedDataIterator;
import java.util.*;

public class ListStats extends GhidraScript {

    // strings that mark the game's own file/asset handling
    private static final String[] INTERESTING = {
        "Not enough memory to play SCREAMER",
        "Error while searching for object file",
        "An error occurred while opening file",
        "File %s is corrupted.",
        "%s.3DX",
        "3D_HEADER",
        "3D_BLKOBJ",
        "Cannot free a visible object",
        "File %s not present in main file.",
        "Out of file domain.",
        "screamer.cfg",
    };

    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();
        ReferenceManager rm = currentProgram.getReferenceManager();

        int nFuncs = 0;
        for (Function f : fm.getFunctions(true)) nFuncs++;
        println("FUNCTIONS: " + nFuncs);
        println("INSTRUCTIONS: " +
            currentProgram.getListing().getNumInstructions());

        for (String target : INTERESTING) {
            byte[] pat = target.getBytes("ISO-8859-1");
            Address hit = mem.findBytes(mem.getMinAddress(), pat, null, true, null);
            if (hit == null) {
                println("\n[missing] " + target);
                continue;
            }
            println("\n=== \"" + target + "\"  @ " + hit);
            for (Reference r : rm.getReferencesTo(hit)) {
                Function f = fm.getFunctionContaining(r.getFromAddress());
                println("      xref from " + r.getFromAddress() +
                        (f != null ? "   in  " + f.getName() + " @ " + f.getEntryPoint()
                                   : "   (no function)"));
            }
        }
    }
}
