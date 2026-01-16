# C89 语法快速参考卡

## ❌ C99 语法（编译错误）

```c
// 1. for 循环内声明
for (int i = 0; i < 10; i++) { ... }

// 2. 代码块中间声明
int result = func();
int x = 10;  // 错误：在语句后声明

// 3. 混合声明和代码
if (condition) {
    doSomething();
    int x = 5;  // 错误
}
```

## ✅ C89 语法（正确写法）

```c
// 1. 变量声明在函数/块开头
{
    int i;
    for (i = 0; i < 10; i++) { ... }
}

// 2. 所有声明放在前面
{
    int x;
    int y;
    int result;
    
    result = func();
    x = 10;
}

// 3. 使用额外代码块
if (condition) {
    int x;  // 在新代码块开头
    doSomething();
    x = 5;
}
```

## 常见错误修复模式

### Pattern 1: for 循环声明
```c
// 错误
void func(void) {
    for (int i = 0; i < 10; i++) {
        printf("%d\n", i);
    }
}

// 修复
void func(void) {
    int i;  // 提升到函数开头
    for (i = 0; i < 10; i++) {
        printf("%d\n", i);
    }
}
```

### Pattern 2: 循环内变量声明
```c
// 错误
while (condition) {
    int temp = getValue();
    process(temp);
}

// 修复
{
    int temp;
    while (condition) {
        temp = getValue();
        process(temp);
    }
}
```

### Pattern 3: switch case 内声明
```c
// 错误
switch (type) {
    case TYPE_A:
        int x = 10;
        break;
}

// 修复
switch (type) {
    case TYPE_A:
        {
            int x = 10;  // 使用代码块
            process(x);
        }
        break;
}
```

## nds32le-elf-gcc 常用选项

```bash
# C89 模式（默认）
nds32le-elf-gcc -c file.c

# 显式指定 C89
nds32le-elf-gcc -std=c89 -c file.c

# 启用 C99 支持
nds32le-elf-gcc -std=c99 -c file.c

# 启用所有警告
nds32le-elf-gcc -Wall -Wextra -c file.c

# 查看预处理输出
nds32le-elf-gcc -E file.c > file.i
```

## 检查清单

- [ ] 所有变量声明在函数/代码块开头
- [ ] for 循环使用预先声明的变量
- [ ] 没有混合声明和代码
- [ ] switch case 内声明使用代码块
- [ ] 没有使用 C99 特性（如 `//` 注释除外）
- [ ] 编译通过且无警告

## 工具辅助

```bash
# 查找所有 for 循环声明
grep -rn "for\s*(\s*(int|uint|char)" *.c

# 查找所有可能的中间声明
grep -rn "^\s\+\(int\|uint\|char\|short\|long\)" *.c
```
