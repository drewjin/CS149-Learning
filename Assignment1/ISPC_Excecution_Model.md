# The ISPC Parallel Execution Model

Though ispc is a C-based language, it is inherently a language for parallel computation. Understanding the details of ispc's parallel execution model that are introduced in this section is critical for writing efficient and correct programs in ispc.

ispc supports two types of parallelism: task parallelism to parallelize across multiple processor cores and SPMD parallelism to parallelize across the SIMD vector lanes on a single core. Most of this section focuses on SPMD parallelism, but see Tasking Model at the end of this section for discussion of task parallelism in ispc.

This section will use some snippets of ispc code to illustrate various concepts. Given ispc's relationship to C, these should be understandable on their own, but you may want to refer to the The ISPC Language section for details on language syntax.

Basic Concepts: Program Instances and Gangs of Program Instances
Upon entry to an ispc function called from C/C++ code, the execution model switches from the application's serial model to ispc's execution model. Conceptually, a number of ispc program instances start running concurrently. The group of running program instances is a called a gang (harkening to "gang scheduling", since ispc provides certain guarantees about the control flow coherence of program instances running in a gang, detailed in Gang Convergence Guarantees.) An ispc program instance is thus similar to a CUDA* "thread" or an OpenCL* "work-item", and an ispc gang is similar to a CUDA* "warp".

An ispc program expresses the computation performed by a gang of program instances, using an "implicit parallel" model, where the ispc program generally describes the behavior of a single program instance, even though a gang of them is actually executing together. This implicit model is the same that is used for shaders in programmable graphics pipelines, OpenCL* kernels, and CUDA*. For example, consider the following ispc function:

```cpp
float func(float a, float b) {
    return a + b / 2.;
}
```

In C, this function describes a simple computation on two individual floating-point values. In ispc, this function describes the computation to be performed by each program instance in a gang. Each program instance has distinct values for the variables a and b, and thus each program instance generally computes a different result when executing this function.

The gang of program instances starts executing in the same hardware thread and context as the application code that called the ispc function; no thread creation or context switching is done under the covers by ispc. Rather, the set of program instances is mapped to the SIMD lanes of the current processor, leading to excellent utilization of hardware SIMD units and high performance.

The number of program instances in a gang is relatively small; in practice, it's no more than 2-4x the native SIMD width of the hardware it is executing on. Thus, four or eight program instances in a gang on a CPU using the 4-wide SSE instruction set, eight or sixteen on a CPU using 8-wide AVX/AVX2, eight, sixteen, thirty two, or sixty four on AVX512 CPU, and eight or sixteen on a Intel GPU.

## Control Flow Within A Gang

Almost all the standard control-flow constructs are supported by ispc; program instances are free to follow different program execution paths than other ones in their gang. For example, consider a simple if statement in ispc code:

```cpp
float x = ..., y = ...;
if (x < y) {
   // true statements
}
else {
   // false statements
}
```

In general, the test x < y may have different result for different program instances in the gang: some of the currently running program instances want to execute the statements for the "true" case and some want to execute the statements for the "false" case.

Complex control flow in ispc programs generally works as expected, computing the same results for each program instance in a gang as would have been computed if the equivalent code ran serially in C to compute each program instance's result individually. However, here we will more precisely define the execution model for control flow in order to be able to precisely define the language's behavior in specific situations.

We will specify the notion of a program counter and how it is updated to step through the program, and an execution mask that indicates which program instances want to execute the instruction at the current program counter. The program counter is shared by all of the program instances in the gang; it points to a single instruction to be executed next. The execution mask is a per-program-instance boolean value that indicates whether or not side effects from the current instruction should effect each program instance. Thus, for example, if a statement were to be executed with an "all off" mask, there should be no observable side-effects.

Upon entry to an ispc function called by the application, the execution mask is "all on" and the program counter points at the first statement in the function. The following two statements describe the required behavior of the program counter and the execution mask over the course of execution of an ispc function.

1. The program counter will have a sequence of values corresponding to a conservative execution path through the function, wherein if any program instance wants to execute a statement, the program counter will pass through that statement.

2. At each statement the program counter passes through, the execution mask will be set such that its value for a particular program instance is "on" if and only if the program instance wants to execute that statement.

Note that these definitions provide the compiler some latitude; for example, the program counter is allowed to pass through a series of statements with the execution mask "all off" because doing so has no observable side-effects.

Elsewhere, we will speak informally of the control flow coherence of a program; this notion describes the degree to which the program instances in the gang want to follow the same control flow path through a function (or, conversely, whether most statements are executed with a "mostly on" execution mask or a "mostly off" execution mask.) In general, control flow divergence leads to reductions in SIMD efficiency (and thus performance) as different program instances want to perform different computations.

## Control Flow Example: If Statements

As a concrete example of the interplay between program counter and execution mask, one way that an if statement like the one in the previous section can be represented is shown by the following pseudo-code compiler output:
```cpp
float x = ..., y = ...;
bool test = (x < y);
mask originalMask = get_current_mask();
set_mask(originalMask & test);
if (any_mask_entries_are_enabled()) {
  // true statements
}
set_mask(originalMask & ~test);
if (any_mask_entries_are_enabled()) {
  // false statements
}
set_mask(originalMask);
```
In other words, the program counter steps through the statements for both the "true" case and the "false" case, with the execution mask set so that no side-effects from the true statements affect the program instances that want to run the false statements, and vice versa. However, a block of statements does not execute if the mask is "all off" upon entry to that block. The execution mask is then restored to the value it had before the if statement.

## Control Flow Example: Loops
for, while, and do statements are handled in an analogous fashion. The program counter continues to run additional iterations of the loop until all of the program instances are ready to exit the loop.

Therefore, if we have a loop like the following:
```cpp
int limit = ...;
for (int i = 0; i < limit; ++i) {
    ...
}
```
where limit has the value 1 for all of the program instances but one, and has value 1000 for the other one, the program counter will step through the loop body 1000 times. The first time, the execution mask will be all on (assuming it is all on going into the for loop), and the remaining 999 times, the mask will be off except for the program instance with a limit value of 1000. (This would be a loop with poor control flow coherence!)

A continue statement in a loop may be handled either by disabling the execution mask for the program instances that execute the continue and then continuing to step the program counter through the rest of the loop, or by jumping to the loop step statement, if all program instances are disabled after the continue has executed. break statements are handled in a similar fashion.

## Gang Convergence Guarantees
The ispc execution model provides an important guarantee about the behavior of the program counter and execution mask: the execution of program instances is maximally converged. Maximal convergence means that if two program instances follow the same control path, they are guaranteed to execute each program statement concurrently. If two program instances follow diverging control paths, it is guaranteed that they will reconverge as soon as possible in the function (if they do later reconverge). [1]

[1]	This is another significant difference between the ispc execution model and the one implemented by OpenCL* and CUDA*, which doesn't provide this guarantee.
Maximal convergence means that in the presence of divergent control flow such as the following:
```cpp
if (test) {
  // true
}
else {
  // false
}
```
It is guaranteed that all program instances that were running before the if test will also be running after the end of the else block. (This guarantee stems from the notion of having a single program counter for the gang of program instances, rather than the concept of a unique program counter for each program instance.)

Another implication of this property is that it would be illegal for the ispc implementation to execute a function with an 8-wide gang by running it two times, with a 4-wide gang representing half of the original 8-wide gang each time.

It also follows that given the following program:
```cpp
if (programIndex == 0) {
    while (true)  // infinite loop
        ;
}
print("hello, world\n");
```
the program will loop infinitely and the print statement will never be executed. (A different execution model that allowed gang divergence might execute the print statement since not all program instances were caught in the infinite loop in the example above.)

The way that "varying" function pointers are handled in ispc is also affected by this guarantee: if a function pointer is varying, then it has a possibly-different value for all running program instances. Given a call to a varying function pointer, ispc must maintain as much execution convergence as possible; the assembly code generated finds the set of unique function pointers over the currently running program instances and calls each one just once, such that the executing program instances when it is called are the set of active program instances that had that function pointer value. The order in which the various function pointers are called in this case is undefined.

## Uniform Data
A variable that is declared with the uniform qualifier represents a single value that is shared across the entire gang. (In contrast, the default variability qualifier for variables in ispc, varying, represents a variable that has a distinct storage location for each program instance in the gang.) (Though see the discussion in Struct Types for some subtleties related to uniform and varying when used with structures.)

It is an error to try to assign a varying value to a uniform variable, though uniform values can be assigned to uniform variables. Assignments to uniform variables are not affected by the execution mask (there's no unambiguous way that they could be); rather, they always apply if the program counter pointer passes through a statement that is a uniform assignment.

## Uniform Control Flow
One advantage of declaring variables that are shared across the gang as uniform, when appropriate, is the reduction in storage space required. A more important benefit is that it can enable the compiler to generate substantially better code for control flow; when a test condition for a control flow decision is based on a uniform quantity, the compiler can be immediately aware that all of the running program instances will follow the same path at that point, saving the overhead of needing to deal with control flow divergence and mask management. (To distinguish the two forms of control flow, will say that control flow based on varying expressions is "varying" control flow.)

Consider for example an image filtering operation where the program loops over pixels adjacent to the given (x,y) coordinates:
```cpp
float box3x3(uniform float image[32][32], int x, int y) {
    float sum = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            sum += image[y+dy][x+dx];
    return sum / 9.;
}
```
In general each program instance in the gang has different values for x and y in this function. For the box filtering algorithm here, all of the program instances will actually want to execute the same number of iterations of the for loops, with all of them having the same values for dx and dy each time through. If these loops are instead implemented with dx and dy declared as uniform variables, then the ispc compiler can generate more efficient code for the loops. [2]

[2]	In this case, a sufficiently smart compiler could determine that dx and dy have the same value for all program instances and thus generate more optimized code from the start, though this optimization isn't yet implemented in ispc.
```cpp
for (uniform int dy = -1; dy <= 1; ++dy)
    for (uniform int dx = -1; dx <= 1; ++dx)
        sum += image[y+dy][x+dx];
```
In particular, ispc can avoid the overhead of checking to see if any of the running program instances wants to do another loop iteration. Instead, the compiler can generate code where all instances always do the same iterations.

The analogous benefit comes when using if statements--if the test in an if statement is based on a uniform test, then the result will by definition be the same for all of the running program instances. Thus, the code for only one of the two cases needs to execute. ispc can generate code that jumps to one of the two, avoiding the overhead of needing to run the code for both cases.

## Uniform Variables and Varying Control Flow

Recall that in the presence of varying control flow, both the "true" and "false" clauses of an if statement may be executed, with the side effects of the instructions masked so that they only apply to the program instances that are supposed to be executing the corresponding clause. Under this model, we must define the effect of modifying uniform variables in the context of varying control flow.

In general, modifying uniform variables under varying control flow leads to the uniform variable having a value that depends on whether any of the program instances in the gang followed a particular execution path. Consider the following example:
```cpp
float a = ...;
uniform int b = 0;
if (a == 0) {
    ++b;
    // b is 1
}
else {
    b = 10;
    // b is 10
}
// whether b is 1 or 10 depends on whether any of the values
// of "a" in the executing gang were 0.
```
Here, if any of the values of a across the gang was non-zero, then b will have a value of 10 after the if statement has executed. However, if all of the values of a in the currently-executing program instances at the start of the if statement had a value of zero, then b would have a value of 1.

Data Races Within a Gang
In order to be able to write well-formed programs where program instances depend on values that are written to memory by other program instances within their gang, it's necessary to have a clear definition of when side-effects from one program instance become visible to other program instances running in the same gang.

In the model implemented by ispc, any side effect from one program instance is visible to other program instances in the gang after the next sequence point in the program. [3]

[3]	This is a significant difference between ispc and SPMD languages like OpenCL* and CUDA*, which require barrier synchronization among the running program instances with functions like barrier() or __syncthreads(), respectively, to ensure this condition.
Generally, sequence points include the end of a full expression, before a function is entered in a function call, at function return, and at the end of initializer expressions. The fact that there is no sequence point between the increment of i and the assignment to i in i=i++ is why the effect that expression is undefined in C, for example. See, for example, the Wikipedia page on sequence points for more information about sequence points in C and C++.

In the following example, we have declared an array of values v, with one value for each running program instance. In the below, assume that programCount gives the gang size, and the varying integer value programIndex indexes into the running program instances starting from zero. (Thus, if 8 program instances are running, the first one of them will have a value 0, the next one a value of 1, and so forth up to 7.)
```cpp
int x = ...;
uniform int tmp[programCount];
tmp[programIndex] = x;
int neighbor = tmp[(programIndex+1)%programCount];
```
In this code, the running program instances have written their values of x into the tmp array such that the ith element of tmp is equal to the value of x for the ith program instance. Then, the program instances load the value of neighbor from tmp, accessing the value written by their neighboring program instance (wrapping around to the first one at the end.) This code is well-defined and without data races, since the writes to and reads from tmp are separated by a sequence point.

(For this particular application of communicating values from one program instance to another, there are more efficient built-in functions in the ispc standard library; see Cross-Program Instance Operations for more information.)

It is possible to write code that has data races across the gang of program instances. For example, if the following function is called with multiple program instances having the same value of index, then it is undefined which of them will write their value of value to array[index].
```cpp
void assign(uniform int array[], int index, int value) {
    array[index] = value;
}
```
As another example, if the values of the array indices i and j have the same values for some of the program instances, and an assignment like the following is performed:
```cpp
int i = ..., j = ...;
uniform int array[...] = { ... };
array[i] = array[j];
```
then the program's behavior is undefined, since there is no sequence point between the reads and writes to the same location.

While this rule that says that program instances can safely depend on side-effects from by other program instances in their gang eliminates a class of synchronization requirements imposed by some other SPMD languages, it conversely means that it is possible to write ispc programs that compute different results when run with different gang sizes.

## Tasking Model
ispc provides an asynchronous function call (i.e. tasking) mechanism through the launch keyword. (The syntax is documented in the Task Parallelism: "launch" and "sync" Statements section.) A function called with launch executes asynchronously from the function that called it; it may run immediately or it may run concurrently on another processor in the system, for example.

If a function launches multiple tasks, there are no guarantees about the order in which the tasks will execute. Furthermore, multiple launched tasks from a single function may execute concurrently.

A function that has launched tasks may use the sync keyword to force synchronization with the launched functions; sync causes a function to wait for all of the tasks it has launched to finish before execution continues after the sync. (Note that sync only waits for the tasks launched by the current function, not tasks launched by other functions).

Alternatively, when a function that has launched tasks returns, an implicit sync waits for all launched tasks to finish before allowing the function to return to its calling function. This feature is important since it enables parallel composition: a function can call second function without needing to be concerned if the second function has launched asynchronous tasks or not--in either case, when the second function returns, the first function can trust that all of its computation has completed.