# Finch for Beginners

**Never written a program before? This guide is for you.**
No experience needed, and no math beyond what you learned in primary school.
Take it slowly, one section at a time, and type every example yourself: that is how it sticks.

> 🇵🇱 Polska wersja: [dla-poczatkujacych.md](../pl/dla-poczatkujacych.md)

---

## Contents

1. [What is a program?](#1-what-is-a-program)
2. [Getting Finch ready](#2-getting-finch-ready)
3. [Your first program](#3-your-first-program)
4. [Showing things on the screen](#4-showing-things-on-the-screen)
5. [Boxes with names: variables](#5-boxes-with-names-variables)
6. [Kinds of values](#6-kinds-of-values)
7. [Doing math](#7-doing-math)
8. [Making decisions: if](#8-making-decisions-if)
9. [Repeating things: loops](#9-repeating-things-loops)
10. [Your own commands: functions](#10-your-own-commands-functions)
11. [When something goes wrong](#11-when-something-goes-wrong)
12. [Small projects to try](#12-small-projects-to-try)
13. [Cheat sheet](#13-cheat-sheet)
14. [Words you will hear](#14-words-you-will-hear)

---

## 1. What is a program?

A computer is very fast, but it does not think. It only does exactly what it is told,
one step at a time.

A **program** is a list of those steps, written down. Like a recipe:

> 1. Take two eggs.
> 2. Break them into a bowl.
> 3. Stir.

A **programming language** is the way we write that recipe so the computer understands it.
**Finch** is one of those languages. It was made to be small and simple, so you
can learn it quickly.

The computer reads your program from top to bottom, line by line.

---

## 2. Getting Finch ready

Finch runs on **Linux** computers. If a teacher, parent or friend has already set it up
for you, skip to the next section.

Otherwise, open the **Terminal** program (a window where you type commands) and type
these lines one by one, pressing **Enter** after each:

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell
cmake -S . -B build -G Ninja
ninja -C build
```

If the last line finishes without the word `error`, Finch is ready.
(If your computer does not have `nix-shell`, ask someone to follow the
[guide for technicians](for-technicians.md#2-installing). It explains the other way.)

To check it works, type:

```sh
./build/finch version
```

You should see something like `finch 1.0.0`.

---

## 3. Your first program

1. Open any text editor (for example *Text Editor*, *Kate*, *gedit* or *VS Code*).
2. Type exactly this:

```c
fn main() {
    print("Hello!")
}
```

3. Save it in the `Finch` folder as **`hello.fn`**. The `.fn` at the end tells everyone it is a Finch program.
4. In the Terminal, type:

```sh
./build/finch run hello.fn
```

You will see:

```
Hello!
```

🎉 **You have just written and run a program.**

### What did we write?

```text
fn main() {            ← "here starts the main part of my program"
    print("Hello!")    ← "show the text Hello! on the screen"
}                      ← "the main part ends here"
```

- `fn main()` is where every Finch program **starts**. Each program has exactly one `main`.
- The curly brackets `{` and `}` hold everything that belongs to `main`, like the two covers of a book.
- `print(...)` shows something on the screen.
- Text always goes inside double quotes: `"Hello!"`.

The spaces at the start of `print` are not required, but they make it easy to see
what is inside the brackets. Always add them. It is a good habit.

---

## 4. Showing things on the screen

`print` can show text and numbers. Put a comma between things, and Finch adds a space for you:

```c
fn main() {
    print("I am", 10, "years old")
    print("2 + 2 =", 2 + 2)
    print()
    print("The line above is empty")
}
```

Shows:

```
I am 10 years old
2 + 2 = 4

The line above is empty
```

Every `print` starts a new line. `print()` with nothing inside makes an empty line.

### Notes for yourself: comments

Anything after `//` is ignored by the computer. Use it to leave notes:

```c
// This program says hi
fn main() {
    print("Hi")   // this shows Hi
}
```

---

## 5. Boxes with names: variables

Imagine a box with a label on it. You can put something in the box and later look
at what is inside by reading the label. In programming that box is called a **variable**.

```c
fn main() {
    age := 10
    print(age)
}
```

`age := 10` means: *"make a new box, label it `age`, and put 10 in it."*

You can change what is in the box later, using just `=`:

```c
fn main() {
    score := 0
    print("Score:", score)

    score = 5
    print("Score:", score)

    score = score + 1
    print("Score:", score)
}
```

```
Score: 0
Score: 5
Score: 6
```

`score = score + 1` reads as: *"take what is in `score`, add 1, and put the result back into `score`."*
There is a shorter way to write the same thing: `score += 1`.

**Remember:**

- `:=` makes a **new** box. You use it only once per name.
- `=` puts a new value into a box **that already exists**.

### Naming your boxes

Names can use the English letters `a`–`z` and `A`–`Z`, digits and `_`, but cannot start with a digit:
`age`, `total_points`, `player2` are fine. `2player` is not.
Letters like `é`, `ü` or `ż` cannot be used in names, only inside text in quotes: `print("Zürich")` is fine.
Pick names that say what is inside: `price` is better than `p`.

---

## 6. Kinds of values

Finch keeps track of what *kind* of thing is in each box. The kind is called a **type**.
These are the ones you will use most:

| Type    | What it holds                | Examples                 |
|---------|------------------------------|--------------------------|
| `int`   | whole numbers                | `0`, `42`, `-7`          |
| `float` | numbers with a decimal point | `3.14`, `0.5`, `-2.0`    |
| `str`   | text                         | `"Hello"`, `"Ola"`       |
| `bool`  | yes or no                    | `true`, `false`          |
| `char`  | a single letter or sign      | `'A'`, `'z'`, `'?'`      |

Finch figures out the type by itself when you use `:=`:

```c
name := "Ola"       // str
age := 10           // int
height := 1.42      // float
likes_cats := true  // bool
```

You can also write the type yourself, in front of the name:

```c
int age = 10
str name = "Ola"
```

Both ways do the same thing. Use the one you like.

**A box keeps its type.** If `age` holds whole numbers, you cannot put text in it later.
Finch will stop and tell you, which helps you catch mistakes early.

---

## 7. Doing math

| Sign | Meaning                    | Example  | Result |
|------|----------------------------|----------|--------|
| `+`  | add                        | `7 + 2`  | `9`    |
| `-`  | subtract                   | `7 - 2`  | `5`    |
| `*`  | multiply                   | `7 * 2`  | `14`   |
| `/`  | divide                     | `7 / 2`  | `3`    |
| `%`  | what is left after dividing | `7 % 2` | `1`    |

**Careful with dividing whole numbers:** `7 / 2` gives `3`, not `3.5`, because both are
whole numbers, so the answer is a whole number too (the part after the dot is dropped).
If you want `3.5`, use a decimal number: `7.0 / 2`.

Multiplication and division happen before adding and subtracting, just like at school.
Use brackets to change the order:

```c
print(2 + 3 * 4)     // 14
print((2 + 3) * 4)   // 20
```

`%` is handy to check if a number is even: an even number divided by 2 leaves nothing,
so `n % 2` is `0`.

You **cannot divide by zero**. If your program tries, it stops and tells you which line did it.

---

## 8. Making decisions: if

Programs can choose what to do. `if` means *"only do this when something is true"*.

```c
fn main() {
    temperature := 25

    if temperature > 20 {
        print("It's warm, wear a t-shirt")
    }
}
```

Add `else` for *"otherwise, do this"*:

```c
if temperature > 20 {
    print("It's warm")
} else {
    print("Take a jacket")
}
```

And `else if` for more choices. Finch checks them from top to bottom and does only
the **first** one that is true:

```c
if temperature > 25 {
    print("Hot!")
} else if temperature > 15 {
    print("Nice")
} else {
    print("Cold")
}
```

### Comparing things

| Sign | Means                       |
|------|-----------------------------|
| `==` | is equal to                 |
| `!=` | is not equal to             |
| `<`  | is smaller than             |
| `>`  | is bigger than              |
| `<=` | is smaller than or equal to |
| `>=` | is bigger than or equal to  |

⚠️ **One `=` puts a value in a box. Two `==` ask "are these equal?"**

You can compare text too: `if name == "Ola" { ... }`.

### Joining questions

- `&&` means **and**: both must be true. `if age > 6 && age < 12 { ... }`
- `||` means **or**: at least one must be true. `if day == "Sat" || day == "Sun" { ... }`
- `!` means **not**: it flips true and false. `if !raining { ... }`

---

## 9. Repeating things: loops

Computers never get bored. A **loop** repeats the same steps many times.

### Counting with `for`

```c
fn main() {
    for i in 1..6 {
        print("Number", i)
    }
}
```

```
Number 1
Number 2
Number 3
Number 4
Number 5
```

`for i in 1..6` means: *"count with `i` from 1, up to but **not including** 6"*.
So `0..10` gives you ten numbers: 0, 1, 2 … 9.

### Repeating while something is true: `while`

```c
fn main() {
    count := 3
    while count > 0 {
        print(count)
        count -= 1
    }
    print("Go!")
}
```

```
3
2
1
Go!
```

⚠️ Make sure the loop can end. If `count` never went down, the loop would run forever.
If that happens, press **Ctrl + C** in the Terminal to stop it.

### Stopping early and skipping

- `break` leaves the loop right away.
- `continue` skips the rest of this round and goes to the next one.

```c
for i in 1..100 {
    if i == 5 {
        break         // stop when we reach 5
    }
    print(i)          // prints 1 2 3 4
}
```

---

## 10. Your own commands: functions

When you do the same thing in many places, give it a name. That named block is a **function**.
You already know one: `main`.

```c
fn say_hello(str name) {
    print("Hello,", name)
}

fn main() {
    say_hello("Ola")
    say_hello("Tom")
}
```

```
Hello, Ola
Hello, Tom
```

`str name` in brackets means: *"this function needs one piece of text, and inside it
we call that text `name`."* The thing you put in brackets when using it is passed in.

### Functions that give back an answer

Write `->` and the type of the answer, and use `return` to give it back:

```c
fn add(int a, int b) -> int {
    return a + b
}

fn main() {
    result := add(2, 3)
    print(result)          // 5
}
```

It does not matter where in the file you put your functions, above or below `main`.

---

## 11. When something goes wrong

Everyone makes mistakes when programming, even experts, every day.
Finch tries hard to explain what went wrong in plain words. Example:

```c
fn main() {
    age := 10
    print(agee)
}
```

```
hello.fn:3:11: error: there is no variable named 'agee'
    3 |     print(agee)
      |           ^
```

How to read it:

- `hello.fn` is the file,
- `3` is the **line** number,
- `11` is how far into the line (the **column**),
- then the explanation, and the line itself with a `^` pointing at the problem.

Here we simply misspelled `age`.

### Common mistakes

| Finch says | What it means | Fix |
|---|---|---|
| `there is no variable named 'x'` | The name is misspelled, or you never made that box | Check spelling. Create it with `x := ...` |
| `a variable named 'x' already exists here` | You used `:=` twice for the same name | Use `=` the second time |
| `'x' must be int, but this is str` | Wrong kind of value for that box | Put the right kind of value in |
| `put each statement on its own line` | Two commands on one line | Press Enter between them |
| `this '{' is never closed` | A `}` is missing | Add the `}` |
| `a condition must be bool` | `if` needs a yes/no question | Compare something: `if x > 0` |
| `runtime error: division by zero` | You divided by 0 while the program ran | Check the number before dividing |

---

## 12. Small projects to try

Try writing each one yourself first. Look at the solution only if you get stuck.

### Project 1: Times table

Show the 7 times table, from 7 × 1 to 7 × 10.

<details><summary>Solution</summary>

```c
fn main() {
    for i in 1..11 {
        print("7 x", i, "=", 7 * i)
    }
}
```
</details>

### Project 2: Even or odd

For every number from 1 to 10, show whether it is even or odd.

<details><summary>Solution</summary>

```c
fn main() {
    for n in 1..11 {
        if n % 2 == 0 {
            print(n, "is even")
        } else {
            print(n, "is odd")
        }
    }
}
```
</details>

### Project 3: Celsius to Fahrenheit

Show a table from 0 °C to 100 °C, every 10 degrees. Formula: `F = C * 9 / 5 + 32`.

<details><summary>Solution</summary>

```c
fn to_fahrenheit(float c) -> float {
    return c * 9 / 5 + 32
}

fn main() {
    for c in 0..11 {
        celsius := float(c * 10)
        print(celsius, "C =", to_fahrenheit(celsius), "F")
    }
}
```

`float(...)` turns a whole number into a decimal number.
</details>

### Project 4: Fizz Buzz

Count from 1 to 20. For numbers divisible by 3 say `Fizz`, by 5 say `Buzz`,
by both say `FizzBuzz`, otherwise say the number.

<details><summary>Solution</summary>

```c
fn main() {
    for n in 1..21 {
        if n % 15 == 0 {
            print("FizzBuzz")
        } else if n % 3 == 0 {
            print("Fizz")
        } else if n % 5 == 0 {
            print("Buzz")
        } else {
            print(n)
        }
    }
}
```
</details>

### Project 5: Biggest of three

Write a function `biggest(int a, int b, int c) -> int` that gives back the largest number.

<details><summary>Solution</summary>

```c
fn biggest(int a, int b, int c) -> int {
    if a >= b && a >= c {
        return a
    }
    if b >= c {
        return b
    }
    return c
}

fn main() {
    print(biggest(4, 9, 2))   // 9
}
```
</details>

---

## 13. Cheat sheet

```c
// a comment

fn main() {                       // the program starts here
    print("text", 42)             // show things

    x := 5                        // new box (type guessed)
    int y = 10                    // new box (type written)
    x = 7                         // change a box
    x += 1                        // add 1 to x

    if x > 5 {                    // decide
        print("big")
    } else if x > 0 {
        print("small")
    } else {
        print("zero or less")
    }

    for i in 0..10 {              // count 0 to 9
        print(i)
    }

    while x > 0 {                 // repeat while true
        x -= 1
    }
}

fn add(int a, int b) -> int {     // your own command
    return a + b
}
```

---

## 14. Words you will hear

| Word | Meaning |
|---|---|
| **program** | A list of steps for the computer |
| **code** | The text of a program |
| **run** | Make the computer do the steps |
| **compile** | Translate your code into something the computer can run. `finch run` does this for you |
| **variable** | A named box holding a value |
| **type** | The kind of value: number, text, yes/no… |
| **function** | A named group of steps you can use again |
| **loop** | Steps that repeat |
| **condition** | A yes/no question, like `x > 5` |
| **error** | A message saying something is wrong, and where |
| **bug** | A mistake in a program |

---

**What next?** When this all feels easy, move on to the
[guide for technicians](for-technicians.md). It covers every part of Finch.
