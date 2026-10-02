// SPDX-License-Identifier: GPL-3.0-or-later
// Rewrites BAPBridge.suppressBapSymbol()Z in the (already byte-patched) rgd_hook.jar so the BAP maneuver
// symbol is blanked ONLY while the altscreen is on the small-dial layout (slot 0 in /tmp/mibr-alt111-lastlayout).
// Missing/unreadable file -> false (symbol kept, i.e. unchanged upstream-with-our-patches behaviour).
// Build: javac -cp asm.jar HudSymbolPatch.java ; run: java -cp asm.jar:. HudSymbolPatch in.jar out.jar
import org.objectweb.asm.*;
import java.io.*;
import java.util.*;
import java.util.zip.*;

public class HudSymbolPatch {
    static final String CLS = "com/mhi2/carplay/rgd/BAPBridge.class";
    public static void main(String[] a) throws Exception {
        byte[] src;
        try (ZipFile z = new ZipFile(a[0])) { src = z.getInputStream(z.getEntry(CLS)).readAllBytes(); }
        ClassReader cr = new ClassReader(src);
        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        final int[] n = {0};
        cr.accept(new ClassVisitor(Opcodes.ASM9, cw) {
            public MethodVisitor visitMethod(int acc, String name, String desc, String sig, String[] ex) {
                final MethodVisitor real = super.visitMethod(acc, name, desc, sig, ex);
                if (!name.equals("suppressBapSymbol") || !desc.equals("()Z")) return real;
                n[0]++;
                return new MethodVisitor(Opcodes.ASM9, null) { // drop the old body entirely
                    public void visitCode() {
                        Label ts = new Label(), te = new Label(), h = new Label(), fin = new Label();
                        real.visitCode();
                        real.visitTryCatchBlock(ts, te, h, "java/lang/Throwable");
                        real.visitLabel(ts);
                        real.visitTypeInsn(Opcodes.NEW, "java/io/FileInputStream");
                        real.visitInsn(Opcodes.DUP);
                        real.visitLdcInsn("/tmp/mibr-alt111-lastlayout");
                        real.visitMethodInsn(Opcodes.INVOKESPECIAL, "java/io/FileInputStream", "<init>", "(Ljava/lang/String;)V", false);
                        real.visitVarInsn(Opcodes.ASTORE, 1);
                        real.visitVarInsn(Opcodes.ALOAD, 1);
                        real.visitMethodInsn(Opcodes.INVOKEVIRTUAL, "java/io/FileInputStream", "read", "()I", false);
                        real.visitVarInsn(Opcodes.ISTORE, 2);
                        real.visitVarInsn(Opcodes.ALOAD, 1);
                        real.visitMethodInsn(Opcodes.INVOKEVIRTUAL, "java/io/FileInputStream", "close", "()V", false);
                        real.visitVarInsn(Opcodes.ILOAD, 2);
                        real.visitIntInsn(Opcodes.BIPUSH, 48); // '0'
                        real.visitJumpInsn(Opcodes.IF_ICMPNE, fin);
                        real.visitLabel(te);
                        real.visitInsn(Opcodes.ICONST_1);
                        real.visitInsn(Opcodes.IRETURN);
                        real.visitLabel(h);
                        real.visitVarInsn(Opcodes.ASTORE, 1);
                        real.visitLabel(fin);
                        real.visitInsn(Opcodes.ICONST_0);
                        real.visitInsn(Opcodes.IRETURN);
                        real.visitMaxs(0, 0);
                        real.visitEnd();
                    }
                };
            }
        }, 0);
        if (n[0] != 1) throw new RuntimeException("suppressBapSymbol()Z found " + n[0] + " times");
        byte[] out = cw.toByteArray();
        try (ZipFile z = new ZipFile(a[0]); ZipOutputStream zo = new ZipOutputStream(new FileOutputStream(a[1]))) {
            for (Enumeration<? extends ZipEntry> e = z.entries(); e.hasMoreElements();) {
                ZipEntry ze = e.nextElement();
                ZipEntry ne = new ZipEntry(ze.getName()); ne.setTime(ze.getTime());
                zo.putNextEntry(ne);
                if (ze.getName().equals(CLS)) zo.write(out); else zo.write(z.getInputStream(ze).readAllBytes());
                zo.closeEntry();
            }
        }
        System.out.println("patched suppressBapSymbol; BAPBridge " + src.length + " -> " + out.length);
    }
}
