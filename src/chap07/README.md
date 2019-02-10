うーん，クラッシュする．

## 再現手順

```
> ./build.sh
warning: unknown warning option '-Wno-class-memaccess'; did you mean
      '-Wno-class-varargs'? [-Wunknown-warning-option]
1 warning generated.
> ./a.out
ready> extern printd(x);
ready> Read extern: 
declare double @printd(double)

ready> printed(1.0);
ready> Error: Unknown function referenced
ready> printd(1.0);
ready> Illegal instruction: 4
```
