// SPDX-License-Identifier: GPL-3.0-or-later
// Stage 2 of the HUD jar build (stage 1 = tools/patch_rgd_hook_jar.py). Takes the stage-1 jar and:
//  A. BAPBridge.suppressBapSymbol()Z      -> true only while /tmp/mibr-alt111-lastlayout starts with '0' (small dials); else false
//  B. BAPBridge.staticOverlayReady()Z     -> true (drops the rgd_blank.png "presentation ready" gate)
//  C. ClusterService.updateDistanceToDestination: Util.formatDistance(I,I,I,String) -> the 5-arg overload this unit has
//  D. ClusterService.hasSatMapProviderChanged()Z -> false (restores stock online-navigation state)
//  E. removes de/audi/tghu/fwhmi/DisplayManagerMIB2High*.class (the stock display manager is left untouched)
// Build: javac -cp asm.jar HudJarPatch.java ; run: java -cp asm.jar:. HudJarPatch in.jar out.jar
import org.objectweb.asm.*;
import java.io.*;
import java.util.*;
import java.util.zip.*;

public class HudJarPatch {
    static final String BAP = "com/mhi2/carplay/rgd/BAPBridge.class";
    static final String CS = "de/audi/tghu/navi/app/cluster/ClusterService.class";
    static final String DM = "de/audi/tghu/fwhmi/DisplayManagerMIB2High";

    // Replaces a method body with `body`; the old instructions are dropped.
    interface Body { void emit(MethodVisitor mv); }
    static ClassVisitor replaceBodies(ClassVisitor cw, Map<String, Body> bodies, int[] hits) {
        return new ClassVisitor(Opcodes.ASM9, cw) {
            public MethodVisitor visitMethod(int acc, String name, String desc, String sig, String[] ex) {
                final MethodVisitor real = super.visitMethod(acc, name, desc, sig, ex);
                Body b = bodies.get(name + desc);
                if (b == null) return real;
                hits[0]++;
                return new MethodVisitor(Opcodes.ASM9, null) {
                    public void visitCode() { real.visitCode(); b.emit(real); real.visitMaxs(0, 0); real.visitEnd(); }
                };
            }
        };
    }
    static void layoutBody(MethodVisitor mv) {
        Label ts = new Label(), te = new Label(), h = new Label(), fin = new Label();
        mv.visitTryCatchBlock(ts, te, h, "java/lang/Throwable");
        mv.visitLabel(ts);
        mv.visitTypeInsn(Opcodes.NEW, "java/io/FileInputStream"); mv.visitInsn(Opcodes.DUP);
        mv.visitLdcInsn("/tmp/mibr-alt111-lastlayout");
        mv.visitMethodInsn(Opcodes.INVOKESPECIAL, "java/io/FileInputStream", "<init>", "(Ljava/lang/String;)V", false);
        mv.visitVarInsn(Opcodes.ASTORE, 1);
        mv.visitVarInsn(Opcodes.ALOAD, 1);
        mv.visitMethodInsn(Opcodes.INVOKEVIRTUAL, "java/io/FileInputStream", "read", "()I", false);
        mv.visitVarInsn(Opcodes.ISTORE, 2);
        mv.visitVarInsn(Opcodes.ALOAD, 1);
        mv.visitMethodInsn(Opcodes.INVOKEVIRTUAL, "java/io/FileInputStream", "close", "()V", false);
        mv.visitVarInsn(Opcodes.ILOAD, 2); mv.visitIntInsn(Opcodes.BIPUSH, 48);
        mv.visitJumpInsn(Opcodes.IF_ICMPNE, fin);
        mv.visitLabel(te);
        mv.visitInsn(Opcodes.ICONST_1); mv.visitInsn(Opcodes.IRETURN);
        mv.visitLabel(h); mv.visitVarInsn(Opcodes.ASTORE, 1);
        mv.visitLabel(fin);
        mv.visitInsn(Opcodes.ICONST_0); mv.visitInsn(Opcodes.IRETURN);
    }
    public static void main(String[] a) throws Exception {
        byte[] bap, cs;
        try (ZipFile z = new ZipFile(a[0])) { bap = z.getInputStream(z.getEntry(BAP)).readAllBytes(); cs = z.getInputStream(z.getEntry(CS)).readAllBytes(); }
        // BAPBridge: A + B
        int[] h1 = {0};
        ClassWriter w1 = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        Map<String, Body> m1 = new HashMap<>();
        m1.put("suppressBapSymbol()Z", HudJarPatch::layoutBody);
        m1.put("staticOverlayReady()Z", mv -> { mv.visitInsn(Opcodes.ICONST_1); mv.visitInsn(Opcodes.IRETURN); });
        new ClassReader(bap).accept(replaceBodies(w1, m1, h1), 0);
        if (h1[0] != 2) throw new RuntimeException("BAPBridge patches applied " + h1[0] + "/2");
        // ClusterService: C + D
        int[] h2 = {0}, c = {0};
        ClassWriter w2 = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        Map<String, Body> m2 = new HashMap<>();
        m2.put("hasSatMapProviderChanged()Z", mv -> { mv.visitInsn(Opcodes.ICONST_0); mv.visitInsn(Opcodes.IRETURN); });
        ClassVisitor v2 = replaceBodies(w2, m2, h2);
        new ClassReader(cs).accept(new ClassVisitor(Opcodes.ASM9, v2) {
            public MethodVisitor visitMethod(int acc, String name, String desc, String sig, String[] ex) {
                MethodVisitor mv = super.visitMethod(acc, name, desc, sig, ex);
                if (mv == null || !name.equals("updateDistanceToDestination")) return mv;
                return new MethodVisitor(Opcodes.ASM9, mv) {
                    public void visitMethodInsn(int op, String ow, String n, String d, boolean itf) {
                        if (ow.equals("de/audi/tghu/navi/app/util/Util") && n.equals("formatDistance") && d.equals("(IIILjava/lang/String;)Ljava/lang/String;")) {
                            c[0]++;
                            super.visitLdcInsn("---"); // stock passes EMPTY_STREET_LABEL twice
                            super.visitMethodInsn(op, ow, n, "(IIILjava/lang/String;Ljava/lang/String;)Ljava/lang/String;", itf);
                        } else super.visitMethodInsn(op, ow, n, d, itf);
                    }
                };
            }
        }, 0);
        if (h2[0] != 1 || c[0] != 1) throw new RuntimeException("ClusterService patches: body " + h2[0] + "/1, formatDistance " + c[0] + "/1");
        int removed = 0;
        try (ZipFile z = new ZipFile(a[0]); ZipOutputStream zo = new ZipOutputStream(new FileOutputStream(a[1]))) {
            for (Enumeration<? extends ZipEntry> e = z.entries(); e.hasMoreElements();) {
                ZipEntry ze = e.nextElement();
                if (ze.getName().startsWith(DM)) { removed++; continue; }
                ZipEntry ne = new ZipEntry(ze.getName()); ne.setTime(ze.getTime());
                zo.putNextEntry(ne);
                byte[] d = ze.getName().equals(BAP) ? w1.toByteArray() : ze.getName().equals(CS) ? w2.toByteArray() : z.getInputStream(ze).readAllBytes();
                zo.write(d); zo.closeEntry();
            }
        }
        if (removed != 3) throw new RuntimeException("removed " + removed + " DisplayManager classes, expected 3");
        System.out.println("ok: A,B (BAPBridge), C,D (ClusterService), E (" + removed + " DisplayManager classes removed)");
    }
}
