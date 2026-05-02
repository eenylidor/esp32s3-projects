// This is a "Header Guard". It prevents the compiler from including
// this file twice, which would cause "redefinition" errors.
#ifndef LOGIC_H
#define LOGIC_H

// This block is crucial for EE/Embedded work.
// It tells the C++ compiler: "The following functions are compiled in pure C style."
// C and C++ handle function names differently in memory (Mangling).
#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Decides what message to show based on button states.
     * @param btn1_pressed 1 if Button 1 is active, 0 otherwise.
     * @param btn2_pressed 1 if Button 2 is active, 0 otherwise.
     * @return A constant pointer to a string (char array).
     */
    const char* get_message(int btn1_pressed, int btn2_pressed);

#ifdef __cplusplus
}
#endif

#endif // LOGIC_H