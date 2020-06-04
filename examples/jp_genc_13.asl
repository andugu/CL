func f(v: array[10] of int)
  var c: array[10] of int
  var i: int

  c = v;

  write "en f. c: ";
  i = 0;
  while i < 10 do
    write c[i]; write ' ';
    i = i+1;
  endwhile
  write '\n';
endfunc


func g(v: array[10] of int)
  var d: array[10] of int
  var i: int
  i = 0;
  while i < 10 do
    d[i] = -1;
    i = i+1;
  endwhile

  v = d;

endfunc


func main()
  var a, b: array[10] of int
  var i, j: int
  i = 0;
  while i < 10 do
    a[i] = i;
    b[i] = 0;
    i = i+1;
  endwhile

  write a[2];
endfunc
