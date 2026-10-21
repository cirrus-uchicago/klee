; RUN: %llvmas %s -o=%t.bc
; RUN: rm -rf %t.klee-out
; RUN: %klee -exit-on-error --output-dir=%t.klee-out --optimize=false %t.bc 2>&1 | FileCheck %s

; The llvm.memcpy has no debug location, as clang emits for a struct passed by
; value. Lowering it to a call to the memcpy below, which has debug info, must
; not break the module.
; CHECK-NOT: Module verification failed
; CHECK: KLEE: done: completed paths = 1

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg)

define ptr @memcpy(ptr %dst, ptr %src, i64 %n) !dbg !10 {
entry:
  %cmp = icmp eq i64 %n, 0, !dbg !12
  br i1 %cmp, label %done, label %loop, !dbg !12

loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %s = getelementptr i8, ptr %src, i64 %i, !dbg !12
  %d = getelementptr i8, ptr %dst, i64 %i, !dbg !12
  %c = load i8, ptr %s, align 1, !dbg !12
  store i8 %c, ptr %d, align 1, !dbg !12
  %next = add i64 %i, 1, !dbg !12
  %end = icmp eq i64 %next, %n, !dbg !12
  br i1 %end, label %done, label %loop, !dbg !12

done:
  ret ptr %dst, !dbg !12
}

define i32 @main() !dbg !13 {
entry:
  %a = alloca [6 x i8], align 1, !dbg !14
  %b = alloca [6 x i8], align 1, !dbg !14
  store i8 7, ptr %a, align 1, !dbg !14
  call void @llvm.memcpy.p0.p0.i64(ptr %b, ptr %a, i64 6, i1 false)
  %v = load i8, ptr %b, align 1, !dbg !14
  %r = zext i8 %v to i32, !dbg !14
  %ret = sub i32 %r, 7, !dbg !14
  ret i32 %ret, !dbg !14
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "memcpy-debug-location.c", directory: "/")
!3 = !{i32 7, !"Dwarf Version", i32 5}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !DISubroutineType(types: !6)
!6 = !{}
!10 = distinct !DISubprogram(name: "memcpy", scope: !1, file: !1, line: 1, type: !5, scopeLine: 1, spFlags: DISPFlagDefinition, unit: !0)
!12 = !DILocation(line: 2, column: 1, scope: !10)
!13 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 5, type: !5, scopeLine: 5, spFlags: DISPFlagDefinition, unit: !0)
!14 = !DILocation(line: 6, column: 1, scope: !13)
