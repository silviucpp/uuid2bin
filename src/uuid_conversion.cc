//https://mariadb.com/kb/en/library/guiduuid-performance/
//https://www.percona.com/blog/2014/12/19/store-uuid-optimized-way/

#include <mysql/mysql.h>
#include <mysql/my_global.h>
#include <string>

#ifdef HAVE_DLOPEN

#define UNUSED(expr) do { (void)(expr); } while (0)

static const uint32_t kUuidLength = 36;
static const uint32_t kUuidLengthNoDash = 32;
static const uint32_t kUuidBinLength = 16;

extern "C" {

    //uuid_to_bin

    my_bool uuid_to_bin_init(UDF_INIT* initid, UDF_ARGS* args, char* message);
    void uuid_to_bin_deinit(UDF_INIT* initid);
    const char* uuid_to_bin(UDF_INIT *initid, UDF_ARGS* args, char* result, unsigned long* length, char* is_null, char* is_error);

    //bin_to_uuid

    my_bool bin_to_uuid_init(UDF_INIT* initid, UDF_ARGS* args, char* message);
    void bin_to_uuid_deinit(UDF_INIT* initid);
    const char* bin_to_uuid(UDF_INIT* initid, UDF_ARGS* args, char* result, unsigned long* length, char* is_null, char* is_error);
}

namespace {

inline int hexchar_to_int(char c)
{
    if (c <= '9' && c >= '0')
        return c-'0';

    c|=32;

    if (c <= 'f' && c >= 'a')
        return c-'a'+10;

    return -1;
}

inline int hexxing(std::string& output, int position, int ch)
{
    static char hexValues[] = "0123456789abcdef";
    output[position++] = hexValues[(ch >> 4) & 0xf];
    output[position++] = hexValues[ch & 0xf];
    return position;
}

bool hexlify_uuid(const char input[kUuidBinLength], std::string& output, bool include_dash)
{
    output.resize(include_dash ? kUuidLength : kUuidLengthNoDash);

    int j = 0;

    if(include_dash)
    {
        for (uint32_t i = 0; i < kUuidBinLength; ++i)
        {
            j = hexxing(output, j, input[i]);

            if(j == 8 || j == 13 || j == 18 || j == 23)
                output[j++] = '-';
        }
    }
    else
    {
        for (uint32_t i = 0; i < kUuidBinLength; ++i)
            j = hexxing(output, j, input[i]);
    }

    return true;
}

bool unhexlify_uuid(const char input[kUuidLengthNoDash], std::string& output)
{
    output.resize(kUuidBinLength);
    int j = 0;

    for (uint32_t i = 0; i < kUuidLengthNoDash; i += 2)
    {
        int highBits = hexchar_to_int(input[i]);
        int lowBits = hexchar_to_int(input[i + 1]);

        if (highBits < 0 || lowBits < 0)
            return false;

        output[j++] = (highBits << 4) + lowBits;
    }

    return true;
}

bool uuid2bin(const std::string& uuid, std::string& out, bool has_dash)
{
    if(uuid.length() != (has_dash ? kUuidLength : kUuidLengthNoDash))
        return false;

    char output[kUuidLengthNoDash];

    if(has_dash)
    {
        memcpy(output, uuid.c_str()+14, 4);
        memcpy(output+4, uuid.c_str()+9, 4);
        memcpy(output+8, uuid.c_str(), 8);
        memcpy(output+16, uuid.c_str()+19, 4);
        memcpy(output+20, uuid.c_str()+24, 12);
    }
    else
    {
        memcpy(output, uuid.c_str()+12, 4);
        memcpy(output+4, uuid.c_str()+8, 4);
        memcpy(output+8, uuid.c_str(), 8);
        memcpy(output+16, uuid.c_str()+16, 4);
        memcpy(output+20, uuid.c_str()+20, 12);
    }

    return unhexlify_uuid(output, out);
}

bool bin2uuid(const std::string& bin, std::string& out, bool include_dash)
{
    if(bin.length() != kUuidBinLength)
        return false;

    char temp[kUuidBinLength];

    memcpy(temp, bin.data()+4, 4);
    memcpy(temp+4, bin.data()+2, 2);
    memcpy(temp+6, bin.data(), 2);
    memcpy(temp+8, bin.data()+8, 2);
    memcpy(temp+10, bin.data()+10, 6);

    return hexlify_uuid(temp, out, include_dash);
}

}

my_bool uuid_to_bin_init(UDF_INIT* initid, UDF_ARGS* args, char* message)
{
    UNUSED(initid);

    if (args->arg_count < 1)
    {
        strcpy(message, "UUID_TO_BIN requires at least one argument");
        return 1;
    }

    if (args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message,"UUID_TO_BIN requires string argument");
        return 1;
    }

    if (args->arg_count == 2 && args->arg_type[1] != INT_RESULT)
    {
        strcpy(message,"UUID_TO_BIN requires second argument as integer");
        return 1;
    }

    initid->maybe_null = 1;
    initid->max_length = kUuidBinLength;
    return 0;
}

void uuid_to_bin_deinit(UDF_INIT* initid)
{
    UNUSED(initid);
}

const char* uuid_to_bin(UDF_INIT* initid, UDF_ARGS* args, char* result, unsigned long* length, char* is_null, char* is_error)
{
    UNUSED(initid);

    if (args->args[0] == NULL)
    {
        *is_null = 1;
        return NULL;
    }

    bool has_dash = args->arg_count == 2 ? *reinterpret_cast<long long*>(args->args[1]) : true;

    std::string uuid(args->args[0], args->lengths[0]);
    std::string out;

    if(!uuid2bin(uuid, out, has_dash))
    {
        *is_error = 1;
        return NULL;
    }

    memcpy(result, out.data(), out.length());
    *length = out.length();
    return result;
}

my_bool bin_to_uuid_init(UDF_INIT* initid, UDF_ARGS* args, char* message)
{
    UNUSED(initid);

    if (args->arg_count < 1)
    {
        strcpy(message, "BIN_TO_UUID requires at least one argument");
        return 1;
    }

    if (args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message,"BIN_TO_UUID requires first argument as binary");
        return 1;
    }

    if (args->arg_count == 2 && args->arg_type[1] != INT_RESULT)
    {
        strcpy(message,"BIN_TO_UUID requires second argument as integer");
        return 1;
    }

    initid->maybe_null = 1;
    initid->max_length = kUuidLength;
    return 0;
}

void bin_to_uuid_deinit(UDF_INIT* initid)
{
    UNUSED(initid);
}

const char* bin_to_uuid(UDF_INIT* initid, UDF_ARGS* args, char* result, unsigned long* length, char* is_null, char* is_error)
{
    UNUSED(initid);

    if (args->args[0] == NULL)
    {
        *is_null = 1;
        return NULL;
    }

    bool include_dash = args->arg_count == 2 ? *reinterpret_cast<long long*>(args->args[1]) : true;

    std::string bin_uuid(args->args[0], args->lengths[0]);
    std::string out;

    if(!bin2uuid(bin_uuid, out, include_dash))
    {
        *is_error = 1;
        return NULL;
    }

    memcpy(result, out.data(), out.length());
    *length = out.length();
    return result;
}

#endif
