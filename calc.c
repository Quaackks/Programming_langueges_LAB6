// Ahmet Yagiz Bulduk (250AHC113)
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used:
// AI (Chat GPT): "Could you explain to me why tokens are used?" Link: https://chatgpt.com/share/6ade5bf8-f8fc-83ed-8432-97e851edc272
// cppreference - fopen documentation
// cppreference - realloc documentation
// cppreference - strtod documentation
// man7 - readdir documentation
// man7 - mkdir documentation
// I found the documents above using AI (Chat GPT)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdint.h>

#define FIRST_NAME "ahmet_yagiz"
#define LAST_NAME "bulduk"
#define STUDENT_ID "250AHC113"
#define START_BUFFER_SIZE 256

// Token types used by the parser (Parser tarafından kullanılan token türleri)
#define TOK_INT 1
#define TOK_FLOAT 2
#define TOK_PLUS 3
#define TOK_MINUS 4
#define TOK_STAR 5
#define TOK_SLASH 6
#define TOK_POWER 7
#define TOK_LPAREN 8
#define TOK_RPAREN 9
#define TOK_END 10
#define TOK_INVALID 11

// Stores one token and its value (bir tokeni ve değerini saklar)
typedef struct Token {
    int type;
    long pos;
    long long int_value;
    double float_value;
} Token;

// Keeps the current state while reading an expression (bir ifadeyi okurken mevcut durumu tutar?)
typedef struct Parser {
    const char *text;
    size_t length;
    size_t index;
    long start_pos;
    int has_newline;

    Token current;
    int error;
    long error_pos;
} Parser;

// Stores either an integer or a floating point result (Tam sayı veya ondalıklı sonucu saklar)
typedef struct Value {
    int is_float;
    long long int_value;
    double float_value;
    long start_pos;
} Value;

// Creates a new token (yeni token oluşturma)
Token make_token(int type, long pos) {
    Token token;

    token.type = type;
    token.pos = pos;
    token.int_value = 0;
    token.float_value = 0.0;

    return token;
}


Value make_int_value(long long value, long pos) {
    Value result;

    result.is_float = 0;
    result.int_value = value;
    result.float_value = (double)value;
    result.start_pos = pos;

    return result;
}


Value make_float_value(double value, long pos) {
    Value result;

    result.is_float = 1;
    result.int_value = 0;
    result.float_value = value;
    result.start_pos = pos;

    return result;
}


double value_as_double(Value value) {
    if (value.is_float) {
        return value.float_value;
    }

    return (double)value.int_value;
}

// Only the first error is saved because the task asks for the first
// position where the parser cannot continue.
void set_error(Parser *parser, long pos) {

    if (parser->error == 0) {
        parser->error = 1;
        parser->error_pos = pos;
    }
}

// Reads the next number or operator from the line (satırdaki bir sonraki sayıyı yada operetörü okur)
Token read_token(Parser *parser) {
    size_t start;
    size_t i;
    long pos;
    Token token;

    // Spaces are skipped as tokens, but index still moves over them.
    // This is needed so error positions match the exact character in the file.
    // Skip spaces but still count their positions (boşlukları geç ama yinede pozisyonlarını say)
    while (parser->index < parser->length &&
           (parser->text[parser->index] == ' ' ||
            parser->text[parser->index] == '\t')) {
        parser->index++;
    }

    if (parser->index >= parser->length) {
        return make_token(TOK_END,
                          parser->start_pos + (long)parser->index);
    }

    // Newline also counts as a character because missing operands or ')' at
    // the end of a line must be reported at the newline position.
    if (parser->has_newline &&
        parser->index == parser->length - 1 &&
        parser->text[parser->index] == '\n') {
        return make_token(TOK_END,
                          parser->start_pos + (long)parser->index);
    }

    start = parser->index;
    pos = parser->start_pos + (long)start;

    if (isdigit((unsigned char)parser->text[start])) {
        unsigned long long integer_number = 0;
        double float_number = 0.0;

        i = start;

        while (i < parser->length &&
               isdigit((unsigned char)parser->text[i])) {
            int digit = parser->text[i] - '0';

            integer_number = integer_number * 10ULL + (unsigned)digit;
            float_number = float_number * 10.0 + digit;
            i++;
        }

        // Check if the number is a float (Sayının ondalıklı olup olmadığını kontrol et)
        if (i + 1 < parser->length &&
            parser->text[i] == '.' &&
            isdigit((unsigned char)parser->text[i + 1])) {
            double place = 0.1;

            i++;

            while (i < parser->length &&
                   isdigit((unsigned char)parser->text[i])) {
                float_number += (parser->text[i] - '0') * place;
                place = place / 10.0;
                i++;
            }

            parser->index = i;

            token = make_token(TOK_FLOAT, pos);
            token.float_value = float_number;
            return token;
        }

        parser->index = i;

        token = make_token(TOK_INT, pos);
        token.int_value = (long long)integer_number;
        return token;
    }

    parser->index++;

    if (parser->text[start] == '+') {
        return make_token(TOK_PLUS, pos);
    }
    else if (parser->text[start] == '-') {
        return make_token(TOK_MINUS, pos);
    }
    else if (parser->text[start] == '/') {
        return make_token(TOK_SLASH, pos);
    }
    else if (parser->text[start] == '(') {
        return make_token(TOK_LPAREN, pos);
    }
    else if (parser->text[start] == ')') {
        return make_token(TOK_RPAREN, pos);
    }
    else if (parser->text[start] == '*') {
        if (parser->index < parser->length &&
            parser->text[parser->index] == '*') {
            parser->index++;
            return make_token(TOK_POWER, pos);
        }

        return make_token(TOK_STAR, pos);
    }

    return make_token(TOK_INVALID, pos);
}


void next_token(Parser *parser) {
    parser->current = read_token(parser);
}

// Calculates power when both values are integers (iki değerde tam sayıysa üs işlemini hesapla)
long long integer_power(long long base, unsigned long long exponent) {
    long long result = 1;

    while (exponent > 0) {
        if (exponent % 2 == 1) {
            result = result * base;
        }

        exponent = exponent / 2;

        if (exponent > 0) {
            base = base * base;
        }
    }

    return result;
}

Value parse_expression(Parser *parser);
// Each precedence level has its own function so ** is evaluated before
// * and /, and * and / are evaluated before + and -.
// Needed because parentheses can contain another expression (parantez içinde başka ifade olabileceği için koydum)
Value parse_primary(Parser *parser) {
    Token token = parser->current;
    Value result;
    long left_parenthesis_pos;

    // Reads numbers and expressions inside parentheses (sayıları ve parantez içindeki ifadeleri okur)
    if (token.type == TOK_INT) {
        next_token(parser);
        return make_int_value(token.int_value, token.pos);
    }

    if (token.type == TOK_FLOAT) {
        next_token(parser);
        return make_float_value(token.float_value, token.pos);
    }

    if (token.type == TOK_LPAREN) {
        left_parenthesis_pos = token.pos;
        next_token(parser);

        result = parse_expression(parser);

        if (parser->error) {
            return result;
        }

        // If ')' is missing, the error is reported at the current token because
        // this is the point where the parser expected the closing parenthesis.
        if (parser->current.type != TOK_RPAREN) {
            set_error(parser, parser->current.pos);
            return result;
        }

        next_token(parser);
        result.start_pos = left_parenthesis_pos;
        return result;
    }

    // An unexpected token is reported at its own position because this is
    // the first character that cannot be used as a valid operand.
    set_error(parser, token.pos);
    return make_int_value(0, token.pos);
}


Value parse_power(Parser *parser) {
    Value left = parse_primary(parser);
    Value right;
    Token operator_token;
    double result;

    if (parser->error) {
        return left;
    }

    if (parser->current.type != TOK_POWER) {
        return left;
    }

    operator_token = parser->current;
    next_token(parser);

    right = parse_power(parser);

    if (parser->error) {
        return left;
    }

    // A negative integer exponent is reported at the start of the exponent
    // because integer power does not allow a negative exponent.
    if (left.is_float == 0 && right.is_float == 0) {
        if (right.int_value < 0) {
            set_error(parser, right.start_pos);
            return left;
        }

        return make_int_value(
            integer_power(left.int_value,
                          (unsigned long long)right.int_value),
            left.start_pos
        );
    }

    errno = 0;
    result = pow(value_as_double(left), value_as_double(right));

    // An invalid floating-point power is reported at the ** operator because
    // the operation itself is what produces the invalid result.
    if (!isfinite(result) || errno == ERANGE || errno == EDOM) {
        set_error(parser, operator_token.pos);
        return left;
    }

    return make_float_value(result, left.start_pos);
}

Value parse_term(Parser *parser) {
    Value left = parse_power(parser);

    // Division by zero is reported at the start of the right operand because
    // this is where the zero value causing the error begins.
    while (parser->error == 0 &&
           (parser->current.type == TOK_STAR ||
            parser->current.type == TOK_SLASH)) {
        Token operator_token = parser->current;
        Value right;

        // Handles multiplication and division (çarpma ve bölme işlemlerini yapar)
        next_token(parser);
        right = parse_power(parser);

        if (parser->error) {
            return left;
        }

        if (operator_token.type == TOK_SLASH) {
            if ((right.is_float == 0 && right.int_value == 0) ||
                (right.is_float == 1 && right.float_value == 0.0)) {
                set_error(parser, right.start_pos);
                return left;
            }
        }

        if (left.is_float == 0 && right.is_float == 0) {
            if (operator_token.type == TOK_STAR) {
                left = make_int_value(left.int_value * right.int_value,
                                      left.start_pos);
            }
            else {
                left = make_int_value(left.int_value / right.int_value,
                                      left.start_pos);
            }
        }
        else {
            double result;

            if (operator_token.type == TOK_STAR) {
                result = value_as_double(left) * value_as_double(right);
            }
            else {
                result = value_as_double(left) / value_as_double(right);
            }

            if (!isfinite(result)) {
                set_error(parser, operator_token.pos);
                return left;
            }

            left = make_float_value(result, left.start_pos);
        }
    }

    return left;
}


Value parse_expression(Parser *parser) {
    Value left = parse_term(parser);

    while (parser->error == 0 &&
           (parser->current.type == TOK_PLUS ||
            parser->current.type == TOK_MINUS)) {
        Token operator_token = parser->current;
        Value right;

        next_token(parser);
        right = parse_term(parser);

        // Handles addition and subtraction (toplama çıkarma işlemlerini yapar)
        if (parser->error) {
            return left;
        }

        if (left.is_float == 0 && right.is_float == 0) {
            if (operator_token.type == TOK_PLUS) {
                left = make_int_value(left.int_value + right.int_value,
                                      left.start_pos);
            }
            else {
                left = make_int_value(left.int_value - right.int_value,
                                      left.start_pos);
            }
        }
        else {
            double result;

            if (operator_token.type == TOK_PLUS) {
                result = value_as_double(left) + value_as_double(right);
            }
            else {
                result = value_as_double(left) - value_as_double(right);
            }

            if (!isfinite(result)) {
                set_error(parser, operator_token.pos);
                return left;
            }

            left = make_float_value(result, left.start_pos);
        }
    }

    return left;
}


// Parses and calculates one line from the input file (input dosyasındaki bir satırı okur yada hesaplar)
int evaluate_line(const char *line, size_t length, long start_pos,
                  Value *result, long *error_pos) {
    Parser parser;
    size_t i = 0;

    while (i < length && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }

    if (i >= length) {
        return 0;
    }

    if (line[i] == '\n') {
        return 0;
    }

    if (line[i] == '#') {
        return 0;
    }

    parser.text = line;
    parser.length = length;
    parser.index = 0;
    parser.start_pos = start_pos;
    parser.error = 0;
    parser.error_pos = 0;

    if (length > 0 && line[length - 1] == '\n') {
        parser.has_newline = 1;
    }
    else {
        parser.has_newline = 0;
    }

    // If a token remains after a complete expression, its position is used
    // because this is the first place where extra invalid input begins.
    next_token(&parser);
    *result = parse_expression(&parser);

    if (parser.error == 0 && parser.current.type != TOK_END) {
        set_error(&parser, parser.current.pos);
    }

    if (parser.error) {
        *error_pos = parser.error_pos;
        return -1;
    }

    return 1;
}

// The file is read one line at a time so the whole file is not stored in memory.
// The buffer is doubled only when needed to avoid repeated small reallocations.
int read_line(FILE *fp, char **line, size_t *capacity, size_t *length) {
    size_t used = 0;
    int ch;

    if (*line == NULL) {
        *capacity = START_BUFFER_SIZE;
        *line = malloc(*capacity);

        if (*line == NULL) {
            return -1;
        }
    }

    // Reads one full line and increases the buffer if needed (tüm satırı okur ve gerekirse bufferı büyütür)
    while ((ch = fgetc(fp)) != EOF) {
        if (used + 1 >= *capacity) {
            char *new_line;
            size_t new_capacity = *capacity * 2;

            new_line = realloc(*line, new_capacity);

            if (new_line == NULL) {
                return -1;
            }

            *line = new_line;
            *capacity = new_capacity;
        }

        (*line)[used] = (char)ch;
        used++;

        if (ch == '\n') {
            break;
        }
    }

    if (ferror(fp)) {
        return -1;
    }

    if (used == 0 && ch == EOF) {
        return 0;
    }

    (*line)[used] = '\0';
    *length = used;

    return 1;
}


int ends_with_txt(const char *name) {
    size_t length = strlen(name);

    if (length <= 4) {
        return 0;
    }

    if (strcmp(name + length - 4, ".txt") == 0) {
        return 1;
    }

    return 0;
}

// Checks if a file name ends with .txt (dosyanın sonu txt ile mi biter kontrol eder)
int is_regular_file(const char *path) {
    struct stat info;

    if (stat(path, &info) != 0) {
        return 0;
    }

    if (S_ISREG(info.st_mode)) {
        return 1;
    }

    return 0;
}

// Creates the output directory if it does not exist (output klosörü yoksa oluşturur)
int make_directory(const char *path) {
    struct stat info;

    if (stat(path, &info) == 0) {
        if (S_ISDIR(info.st_mode)) {
            return 0;
        }

        return -1;
    }

    if (mkdir(path, 0777) != 0) {
        return -1;
    }

    return 0;
}


char *copy_part(const char *text, size_t length) {
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    copy[length] = '\0';

    return copy;
}


char *get_base_name(const char *path) {
    size_t end = strlen(path);
    size_t start;

    while (end > 0 && path[end - 1] == '/') {
        end--;
    }

    if (end == 0) {
        return copy_part(".", 1);
    }

    start = end;

    while (start > 0 && path[start - 1] != '/') {
        start--;
    }

    return copy_part(path + start, end - start);
}


char *remove_txt(const char *path) {
    char *name = get_base_name(path);
    size_t length;

    if (name == NULL) {
        return NULL;
    }

    length = strlen(name);

    if (length > 4 && strcmp(name + length - 4, ".txt") == 0) {
        name[length - 4] = '\0';
    }

    return name;
}


char *join_path(const char *folder, const char *name) {
    size_t folder_length = strlen(folder);
    size_t name_length = strlen(name);
    int need_slash = 1;
    char *path;

    if (folder_length > 0 && folder[folder_length - 1] == '/') {
        need_slash = 0;
    }

    path = malloc(folder_length + name_length + (size_t)need_slash + 1);

    if (path == NULL) {
        return NULL;
    }

    // Joins a folder name and file name (klasör ve dosya adını birleştir)
    strcpy(path, folder);

    if (need_slash) {
        strcat(path, "/");
    }

    strcat(path, name);

    return path;
}


char *make_output_filename(const char *input_path) {
    char *base = remove_txt(input_path);
    char *name;
    size_t size;

    if (base == NULL) {
        return NULL;
    }

    size = strlen(base) + strlen(FIRST_NAME) + strlen(LAST_NAME) +
           strlen(STUDENT_ID) + 8;

    name = malloc(size);

    // Creates the required output file name (gerekli dosya ve output adını oluşturur)
    if (name != NULL) {
        snprintf(name, size, "%s_%s_%s_%s.txt",
                 base, FIRST_NAME, LAST_NAME, STUDENT_ID);
    }

    free(base);
    return name;
}

// Creates the default output folder name (varsayılan output klasör adını oluşturur)
char *make_default_output_dir(const char *source, int directory_mode) {
    char *base;
    char *name;
    size_t size;

    if (directory_mode) {
        base = get_base_name(source);
    }
    else {
        base = remove_txt(source);
    }

    if (base == NULL) {
        return NULL;
    }

    size = strlen(base) + strlen(FIRST_NAME) + strlen(STUDENT_ID) + 4;
    name = malloc(size);

    if (name != NULL) {
        snprintf(name, size, "%s_%s_%s",
                 base, FIRST_NAME, STUDENT_ID);
    }

    free(base);
    return name;
}


void print_usage(const char *message) {
    fprintf(stderr, "calc: %s\n", message);
    fprintf(stderr,
            "usage: calc [-d DIR | --dir DIR] "
            "[-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
}


int evaluate_file(const char *input_path, const char *output_dir) {
    FILE *input;
    FILE *output;
    char *output_name;
    char *output_path;
    char *line = NULL;
    size_t capacity = 0;
    size_t length = 0;
    long file_pos = 1;
    int read_result;

    input = fopen(input_path, "rb");

    // Reads the input file and writes calculated results (input dosyasını okur ve sonuçları yazar)
    if (input == NULL) {
        fprintf(stderr, "calc: cannot open input file '%s'\n", input_path);
        return -1;
    }

    output_name = make_output_filename(input_path);

    if (output_name == NULL) {
        fclose(input);
        return -1;
    }

    output_path = join_path(output_dir, output_name);
    free(output_name);

    if (output_path == NULL) {
        fclose(input);
        return -1;
    }

    output = fopen(output_path, "wb");

    if (output == NULL) {
        fprintf(stderr, "calc: cannot write output file '%s'\n", output_path);
        free(output_path);
        fclose(input);
        return -1;
    }

    // Earlier results are removed when an error is found because the task
    // requires the output file to contain only ERROR:<pos>.
    while ((read_result = read_line(input, &line, &capacity, &length)) > 0) {
        Value value;
        long error_pos = 0;
        int line_result;

        line_result = evaluate_line(line, length, file_pos,
                                    &value, &error_pos);

        if (line_result < 0) {

            fclose(output);
            output = fopen(output_path, "wb");

            if (output == NULL) {
                fprintf(stderr, "calc: cannot rewrite output file '%s'\n",
                        output_path);
                free(line);
                free(output_path);
                fclose(input);
                return -1;
            }

            // If there is an error, output only ERROR:<pos> (hata varsa error pos yaz)
            fprintf(output, "ERROR:%ld\n", error_pos);

            if (fclose(output) != 0) {
                free(line);
                free(output_path);
                fclose(input);
                return -1;
            }

            free(line);
            free(output_path);
            fclose(input);
            return 0;
        }

        if (line_result > 0) {
            if (value.is_float) {
                fprintf(output, "%.15g\n", value.float_value);
            }
            else {
                fprintf(output, "%lld\n", value.int_value);
            }
        }

        if (ferror(output)) {
            fprintf(stderr, "calc: write error for '%s'\n", output_path);
            fclose(output);
            free(line);
            free(output_path);
            fclose(input);
            return -1;
        }

        file_pos = file_pos + (long)length;
    }

    if (read_result < 0) {
        fprintf(stderr, "calc: read error for '%s'\n", input_path);
        fclose(output);
        free(line);
        free(output_path);
        fclose(input);
        return -1;
    }

    if (fclose(output) != 0) {
        free(line);
        free(output_path);
        fclose(input);
        return -1;
    }

    free(line);
    free(output_path);
    fclose(input);

    return 0;
}

// Processes every .txt file inside the directory (klasördeki bütün txt dosyalarını inceler)
int evaluate_directory(const char *folder, const char *output_dir) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(folder);

    if (dir == NULL) {
        fprintf(stderr, "calc: cannot open directory '%s'\n", folder);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *path;

        if (ends_with_txt(entry->d_name) == 0) {
            continue;
        }

        path = join_path(folder, entry->d_name);

        if (path == NULL) {
            closedir(dir);
            return -1;
        }

        if (is_regular_file(path)) {
            if (evaluate_file(path, output_dir) != 0) {
                free(path);
                closedir(dir);
                return -1;
            }
        }

        free(path);
    }

    closedir(dir);
    return 0;
}


int main(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *input_dir = NULL;
    const char *output_dir = NULL;
    char *default_output_dir = NULL;
    int i;
    int result;

    // Reads command line options and starts the program (komut satırı seçeneklerini okur ve programı başlatır)
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (i + 1 >= argc) {
                print_usage("missing directory after -d/--dir");
                return EXIT_FAILURE;
            }

            // Check command line arguments (komut satırı argümanlarını kontrol et)
            if (input_dir != NULL) {
                print_usage("directory given more than once");
                return EXIT_FAILURE;
            }

            input_dir = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-o") == 0 ||
                 strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc) {
                print_usage("missing directory after -o/--output-dir");
                return EXIT_FAILURE;
            }

            if (output_dir != NULL) {
                print_usage("output directory given more than once");
                return EXIT_FAILURE;
            }

            output_dir = argv[i + 1];
            i++;
        }
        else if (argv[i][0] == '-') {
            print_usage("unknown option");
            return EXIT_FAILURE;
        }
        else {
            if (input_file != NULL) {
                print_usage("too many input files");
                return EXIT_FAILURE;
            }

            input_file = argv[i];
        }
    }

    if (input_file == NULL && input_dir == NULL) {
        print_usage("input file or -d directory is required");
        return EXIT_FAILURE;
    }

    if (input_file != NULL && input_dir != NULL) {
        print_usage("use either one input file or -d, not both");
        return EXIT_FAILURE;
    }

    if (output_dir == NULL) {
        if (input_dir != NULL) {
            default_output_dir = make_default_output_dir(input_dir, 1);
        }
        else {
            default_output_dir = make_default_output_dir(input_file, 0);
        }

        if (default_output_dir == NULL) {
            fprintf(stderr, "calc: memory allocation failed\n");
            return EXIT_FAILURE;
        }

        output_dir = default_output_dir;
    }

    if (make_directory(output_dir) != 0) {
        fprintf(stderr, "calc: cannot create or use output directory '%s'\n",
                output_dir);
        free(default_output_dir);
        return EXIT_FAILURE;
    }

    if (input_dir != NULL) {
        result = evaluate_directory(input_dir, output_dir);
    }
    else {
        result = evaluate_file(input_file, output_dir);
    }

    free(default_output_dir);

    if (result != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}