#include "kde_display_scale.h"
#include <systemd/sd-bus.h>
#include <SDL.h>
#include <cstring>
#include <string>

namespace
{
    // Skips over the value of a dict-entry's variant, whatever type it turns
    // out to be (scalar, nested array/struct, ...). Used for every field we
    // don't care about -- getConfig's payload is large (every output lists
    // every supported mode), and enumerating fields we don't need generically
    // is far less fragile than hand-parsing the full schema.
    void skipVariantValue(sd_bus_message* m)
    {
        const char* contents = nullptr;
        char type = 0;
        if (sd_bus_message_peek_type(m, &type, &contents) <= 0) return;
        if (sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, contents) < 0) return;
        sd_bus_message_skip(m, contents);
        sd_bus_message_exit_container(m);
    }

    // "scale" comes back as a double when fractional (e.g. 2.058333...) but
    // as an int64 when it's exactly a whole number (e.g. 1) -- observed
    // directly against a live getConfig call, not documented anywhere, so
    // both variant signatures need handling.
    bool readVariantAsDouble(sd_bus_message* m, double& out)
    {
        const char* contents = nullptr;
        char type = 0;
        if (sd_bus_message_peek_type(m, &type, &contents) <= 0) return false;
        if (sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, contents) < 0) return false;

        bool ok = false;
        if (strcmp(contents, "d") == 0)
        {
            double d = 0;
            if (sd_bus_message_read(m, "d", &d) >= 0) { out = d; ok = true; }
        }
        else if (strcmp(contents, "x") == 0)
        {
            int64_t x = 0;
            if (sd_bus_message_read(m, "x", &x) >= 0) { out = static_cast<double>(x); ok = true; }
        }
        else
        {
            sd_bus_message_skip(m, contents);
        }

        sd_bus_message_exit_container(m);
        return ok;
    }

    bool readVariantAsString(sd_bus_message* m, std::string& out)
    {
        const char* contents = nullptr;
        char type = 0;
        if (sd_bus_message_peek_type(m, &type, &contents) <= 0) return false;
        if (sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, contents) < 0) return false;

        bool ok = false;
        if (strcmp(contents, "s") == 0)
        {
            const char* s = nullptr;
            if (sd_bus_message_read(m, "s", &s) >= 0 && s) { out = s; ok = true; }
        }
        else
        {
            sd_bus_message_skip(m, contents);
        }

        sd_bus_message_exit_container(m);
        return ok;
    }

    // Reads one output entry (an a{sv} nested inside the "outputs" av) and
    // returns its "name" and "scale" fields, skipping everything else.
    void readOutputEntry(sd_bus_message* m, std::string& name, double& scale, bool& haveScale)
    {
        sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}");

        int r;
        while ((r = sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv")) > 0)
        {
            const char* key = nullptr;
            sd_bus_message_read(m, "s", &key);

            if (key && strcmp(key, "name") == 0)
            {
                readVariantAsString(m, name);
            }
            else if (key && strcmp(key, "scale") == 0)
            {
                haveScale = readVariantAsDouble(m, scale);
            }
            else
            {
                skipVariantValue(m);
            }

            sd_bus_message_exit_container(m); // dict entry
        }

        sd_bus_message_exit_container(m); // a{sv} array
    }
}

float queryKdeOutputScale(const char* connectorName)
{
    const float fallback = 1.0f;
    if (!connectorName) return fallback;

    sd_bus* bus = nullptr;
    if (sd_bus_open_user(&bus) < 0) return fallback;

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    int r = sd_bus_call_method(bus, "org.kde.KScreen", "/backend",
                                "org.kde.kscreen.Backend", "getConfig",
                                &error, &reply, "");
    if (r < 0)
    {
        SDL_Log("KDE display scale: getConfig call failed (%s), using scale 1.0", error.message);
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
        return fallback;
    }

    float result = fallback;
    bool found = false;

    // Top-level reply is a{sv}.
    if (sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "{sv}") >= 0)
    {
        while (!found && sd_bus_message_enter_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv") > 0)
        {
            const char* key = nullptr;
            sd_bus_message_read(reply, "s", &key);

            if (key && strcmp(key, "outputs") == 0)
            {
                const char* contents = nullptr;
                char type = 0;
                sd_bus_message_peek_type(reply, &type, &contents);
                if (sd_bus_message_enter_container(reply, SD_BUS_TYPE_VARIANT, contents) >= 0)
                {
                    if (sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "v") >= 0)
                    {
                        while (sd_bus_message_enter_container(reply, SD_BUS_TYPE_VARIANT, "a{sv}") > 0)
                        {
                            std::string name;
                            double scale = 1.0;
                            bool haveScale = false;
                            readOutputEntry(reply, name, scale, haveScale);

                            if (name == connectorName && haveScale)
                            {
                                result = static_cast<float>(scale);
                                found = true;
                            }

                            sd_bus_message_exit_container(reply); // variant
                        }
                        sd_bus_message_exit_container(reply); // array of v
                    }
                    sd_bus_message_exit_container(reply); // outputs variant
                }
            }
            else
            {
                skipVariantValue(reply);
            }

            sd_bus_message_exit_container(reply); // dict entry
        }
        sd_bus_message_exit_container(reply); // top-level a{sv}
    }

    if (!found)
    {
        SDL_Log("KDE display scale: no output named '%s' in KScreen config, using scale 1.0",
                connectorName);
    }

    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    return result;
}
