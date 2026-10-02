// SPDX-License-Identifier: GPL-3.0-or-later
// Linkage audit: does every HMI-class member the jar references exist in this unit's stock classes?
// usage: java -cp asm.jar:. LinkCheck jar stock.jar
import org.objectweb.asm.*;
import java.io.*;
import java.util.*;
import java.util.zip.*;

public class LinkCheck {
    static ZipFile stock, jar;
    static Map<String, ClassInfo> cache = new HashMap<>();
    static class ClassInfo { String sup; String[] ifs; Set<String> members = new HashSet<>(); boolean found; }
    static ClassInfo load(String n) {
        if (cache.containsKey(n)) return cache.get(n);
        ClassInfo ci = new ClassInfo(); cache.put(n, ci);
        try {
            ZipEntry e = stock.getEntry(n + ".class");
            if (e == null) return ci;
            ClassReader cr = new ClassReader(stock.getInputStream(e));
            ci.found = true;
            cr.accept(new ClassVisitor(Opcodes.ASM9) {
                public void visit(int v, int a, String name, String sig, String sup, String[] ifs) { ci.sup = sup; ci.ifs = ifs == null ? new String[0] : ifs; }
                public MethodVisitor visitMethod(int a, String name, String d, String s, String[] ex) { ci.members.add("M " + name + d); return null; }
                public FieldVisitor visitField(int a, String name, String d, String s, Object v) { ci.members.add("F " + name + " " + d); return null; }
            }, ClassReader.SKIP_CODE);
        } catch (IOException ex) { }
        return ci;
    }
    static boolean resolve(String owner, String key, int depth) {
        if (owner == null || depth > 30) return false;
        ClassInfo ci = load(owner);
        if (!ci.found) return false;
        if (ci.members.contains(key)) return true;
        if (resolve(ci.sup, key, depth + 1)) return true;
        for (String i : ci.ifs) if (resolve(i, key, depth + 1)) return true;
        return false;
    }
    public static void main(String[] a) throws Exception {
        jar = new ZipFile(a[0]); stock = new ZipFile(a[1]);
        Set<String> jarClasses = new HashSet<>();
        for (Enumeration<? extends ZipEntry> en = jar.entries(); en.hasMoreElements();) { String n = en.nextElement().getName(); if (n.endsWith(".class")) jarClasses.add(n.substring(0, n.length() - 6)); }
        Set<String> overridden = new TreeSet<>();
        for (String c : jarClasses) if (stock.getEntry(c + ".class") != null) overridden.add(c);
        System.out.println("jar classes: " + jarClasses.size() + "; overriding stock classes: " + overridden);
        // Build the "effective" view: jar classes shadow stock. For refs into overridden classes, check the jar copy's members.
        Map<String, Set<String>> jarMembers = new HashMap<>();
        for (String c : jarClasses) {
            Set<String> m = new HashSet<>(); jarMembers.put(c, m);
            new ClassReader(jar.getInputStream(jar.getEntry(c + ".class"))).accept(new ClassVisitor(Opcodes.ASM9) {
                public MethodVisitor visitMethod(int ac, String name, String d, String s, String[] ex) { m.add("M " + name + d); return null; }
                public FieldVisitor visitField(int ac, String name, String d, String s, Object v) { m.add("F " + name + " " + d); return null; }
            }, ClassReader.SKIP_CODE);
        }
        TreeMap<String, TreeSet<String>> bad = new TreeMap<>();
        TreeSet<String> unchecked = new TreeSet<>();
        for (String c : jarClasses) {
            final String cc = c;
            new ClassReader(jar.getInputStream(jar.getEntry(c + ".class"))).accept(new ClassVisitor(Opcodes.ASM9) {
                public MethodVisitor visitMethod(int ac, String mname, String md, String s, String[] ex) {
                    final String where = cc + "." + mname + md;
                    return new MethodVisitor(Opcodes.ASM9) {
                        void chk(String owner, String name, String desc, boolean field) {
                            if (owner.startsWith("[") || owner.startsWith("java/") || owner.startsWith("javax/")) return;
                            String key = field ? "F " + name + " " + desc : "M " + name + desc;
                            if (jarClasses.contains(owner)) {
                                if (jarMembers.get(owner).contains(key)) return;
                                // inherited from stock super? only matters for overridden classes
                                if (!resolve(owner, key, 0)) bad.computeIfAbsent(owner + "." + name + desc, k -> new TreeSet<>()).add(where);
                                return;
                            }
                            ClassInfo ci = load(owner);
                            if (!ci.found) { unchecked.add(owner); return; }
                            if (!resolve(owner, key, 0)) bad.computeIfAbsent(owner + "." + name + desc, k -> new TreeSet<>()).add(where);
                        }
                        public void visitMethodInsn(int o, String ow, String n, String d, boolean i) { chk(ow, n, d, false); }
                        public void visitFieldInsn(int o, String ow, String n, String d) { chk(ow, n, d, true); }
                    };
                }
            }, 0);
        }
        System.out.println("\nUNRESOLVED member references against this unit's stock classes: " + bad.size());
        for (Map.Entry<String, TreeSet<String>> e : bad.entrySet()) {
            System.out.println("  " + e.getKey());
            int k = 0; for (String w : e.getValue()) { if (k++ < 4) System.out.println("      from " + w); }
            if (e.getValue().size() > 4) System.out.println("      ... +" + (e.getValue().size() - 4) + " more");
        }
        System.out.println("\nowners not present in lsd.jxe (not checked): " + unchecked.size());
        for (String u : unchecked) System.out.println("  " + u);
    }
}
