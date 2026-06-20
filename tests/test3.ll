define i32 @test3() {
entry:
br label %loop
loop:
%i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
%i.next = add i32 %i, 1
%done = icmp sge i32 %i.next, 10
br i1 %done, label %exit, label %loop
exit:
ret i32 %i
}