define i32 @test1() {
entry:
%a = add i32 2, 3        ; treba: Constant(5)
%b = mul i32 %a, 10      ; treba: Constant(50)
%cond = icmp eq i32 %b, 50  ; treba: Constant(true)
br i1 %cond, label %taken, label %nottaken

taken:
%x = phi i32 [ 1, %entry ]   ; samo iz entry, treba: Constant(1)
ret i32 %x

nottaken:                       ; nikad executable, treba ostati Top za sve unutra
%y = phi i32 [ 99, %entry ]
ret i32 %y
}