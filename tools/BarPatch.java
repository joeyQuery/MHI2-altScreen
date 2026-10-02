import org.objectweb.asm.*;
import java.io.*;
import java.util.*;
import java.util.zip.*;

public class BarPatch {
    public static void main(String[] a) throws Exception {
        String in = a[0], out = a[1];
        String cls = "de/audi/tghu/navi/app/cluster/CombiBAPListener.class";
        Set<String> kill = new HashSet<>(Arrays.asList(a[2].split(",")));
        byte[] src;
        try (ZipFile z = new ZipFile(in)) { src = z.getInputStream(z.getEntry(cls)).readAllBytes(); }
        ClassReader cr = new ClassReader(src);
        ClassWriter cw = new ClassWriter(0);
        final int[] n = {0};
        cr.accept(new ClassVisitor(Opcodes.ASM9, cw) {
            public MethodVisitor visitMethod(int acc, String name, String desc, String sig, String[] ex) {
                MethodVisitor mv = super.visitMethod(acc, name, desc, sig, ex);
                if (kill.contains(name) && desc.equals("()V")) {
                    n[0]++;
                    return new MethodVisitor(Opcodes.ASM9, mv) {
                        public void visitCode() { super.visitCode(); super.visitInsn(Opcodes.RETURN); }
                    };
                }
                return mv;
            }
        }, 0);
        if (n[0] != kill.size()) throw new RuntimeException("patched " + n[0] + " of " + kill.size());
        byte[] patched = cw.toByteArray();
        // deterministic stored jar, same layout as MIBR-NavIgnore.jar
        try (ZipOutputStream zo = new ZipOutputStream(new FileOutputStream(out))) {
            zo.setMethod(ZipOutputStream.STORED);
            byte[] mf = ("Manifest-Version: 1.0\r\nImplementation-Title: MIBR ClusterBar patch\r\nCreated-By: AltScreen MU0678\r\n\r\n").getBytes("UTF-8");
            put(zo, "META-INF/MANIFEST.MF", mf);
            put(zo, cls, patched);
        }
        System.out.println("patched methods: " + n[0] + ", class bytes " + src.length + " -> " + patched.length);
    }
    static void put(ZipOutputStream zo, String name, byte[] d) throws IOException {
        ZipEntry e = new ZipEntry(name);
        e.setMethod(ZipEntry.STORED); e.setSize(d.length); e.setCompressedSize(d.length);
        CRC32 c = new CRC32(); c.update(d); e.setCrc(c.getValue());
        e.setTime(315532800000L);
        zo.putNextEntry(e); zo.write(d); zo.closeEntry();
    }
    static class CRC32 extends java.util.zip.CRC32 {}
}
