//Give functions that take arguments on the STACK their real signature, so that the
//decompiler shows the callers passing them (it drops a float pushed for a __watcall
//function it believes takes registers only).
//usage: -postScript FixSigs.java <file>     lines:  <entry> <n register args> <stack types...>
//  e.g.  0x19810 1 float        = (EAX, float at [esp+4])
//        0x1016b 1 float int    = (EAX, float at +4, int at +8)
//        0x14dc0 0 float float ->float   = (two floats on the stack), the result in ST0
//Register arguments are EAX, EDX, EBX, ECX in that order, all int.
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.*;
import java.util.*;

public class FixSigs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] regs = { "EAX", "EDX", "EBX", "ECX" };
        int n = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            line = line.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] w = line.split("\\s+");
            Function f = currentProgram.getFunctionManager().getFunctionAt(toAddr(Long.decode(w[0])));
            if (f == null) { println("SIGS: no function at " + w[0]); continue; }
            /* `inline`: a few instructions that hand registers on (the library's abs): let the
               decompiler look through the call */
            if (w[1].equals("inline")) { f.setInline(true); n++; continue; }
            List<Variable> ps = new ArrayList<>();
            int nreg = Integer.parseInt(w[1]);
            for (int i = 0; i < nreg; i++)
                ps.add(new ParameterImpl("r" + i, IntegerDataType.dataType,
                        new VariableStorage(currentProgram, currentProgram.getRegister(regs[i])), currentProgram));
            int off = 4;
            Parameter ret = null;
            boolean intResult = false;
            for (int i = 2; i < w.length; i++) {
                if (w[i].equals("->float")) {         /* it leaves its result in ST0 */
                    ret = new ReturnParameterImpl(Float10DataType.dataType,
                            new VariableStorage(currentProgram, currentProgram.getRegister("ST0")), currentProgram);
                    continue;
                }
                if (w[i].equals("->int")) {           /* a whole number in EAX, and nothing else touched */
                    ret = new ReturnParameterImpl(IntegerDataType.dataType,
                            new VariableStorage(currentProgram, currentProgram.getRegister("EAX")), currentProgram);
                    intResult = true;
                    continue;
                }
                DataType dt = w[i].equals("float") ? FloatDataType.dataType : w[i].equals("double") ? DoubleDataType.dataType : IntegerDataType.dataType;
                ps.add(new ParameterImpl("s" + (i - 2), dt, new VariableStorage(currentProgram, off, dt.getLength()), currentProgram));
                off += dt.getLength();
            }
            if (nreg == 0) {
                /* everything on the stack: the compiler spec's __cdecl says so, and where a
                   float result is (custom storage in ST0 is not taken up by the callers) */
                List<Variable> dyn = new ArrayList<>();
                for (Variable v : ps) dyn.add(new ParameterImpl(v.getName(), v.getDataType(), currentProgram));
                Parameter r = new ReturnParameterImpl(ret != null ? DoubleDataType.dataType : VoidDataType.dataType, currentProgram);
                f.setCustomVariableStorage(false);
                f.updateFunction("__cdecl", r, dyn, Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED);
                n++;
                continue;
            }
            if (ret != null && !intResult) {
                /* registers AND the stack AND a float result: the default convention with
                   all four register parameters written out (those not used stay names),
                   so that what follows them falls on the stack and the result in ST0 */
                List<Variable> dyn = new ArrayList<>();
                for (int i = 0; i < 4; i++) dyn.add(new ParameterImpl("r" + i, IntegerDataType.dataType, currentProgram));
                for (int i = nreg; i < ps.size(); i++) dyn.add(new ParameterImpl(ps.get(i).getName(), ps.get(i).getDataType(), currentProgram));
                f.setCustomVariableStorage(false);
                f.updateFunction("__watcall", new ReturnParameterImpl(DoubleDataType.dataType, currentProgram), dyn,
                        Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED);
                n++;
                continue;
            }
            if (ret == null) ret = new ReturnParameterImpl(VoidDataType.dataType, VariableStorage.VOID_STORAGE, currentProgram);
            f.updateFunction(null, ret, ps, Function.FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED);
            n++;
        }
        println("SIGS: " + n + " signatures set");
    }
}
