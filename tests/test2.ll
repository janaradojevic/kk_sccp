define i32 @test2(i1 %unknown) {
entry:
br i1 %unknown, label %A, label %B
A:
br label %merge
B:
br label %merge
merge:
%v = phi i32 [ 1, %A ], [ 2, %B ]
ret i32 %v
}