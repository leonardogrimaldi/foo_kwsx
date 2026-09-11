#include "stdafx.h"
#include <foobar2000/SDK/foobar2000.h>
#include "json.hpp"
using json = nlohmann::json;

namespace {
    const char* KWSX_URL = "https://stream.kwsx.online/listen/kwsx/radio.mp3";
    const char* NOW_PLAYING = "https://stream.kwsx.online/api/nowplaying/kwsx";

    static ui_status_text_override::ptr override_obj;

    class my_static_monitor : public play_callback_static {
    public:
        // 1. MUST be implemented. Queried ONCE by foobar2000 on startup.
        unsigned get_flags() override {
            return flag_on_playback_new_track | flag_on_playback_stop;
        }

        // 2. Override the events you subscribed to in get_flags()
        void on_playback_new_track(metadb_handle_ptr p_track) override {
            console::print("New track playing!");
            check_stream(p_track);
            
        }

        void on_playback_stop(play_control::t_stop_reason p_reason) override {
            console::print("Playback stopped.");
        }

		void on_playback_dynamic_info_track(const file_info& p_info) override {
			console::print("Dynamic info track updated.");
            /*const char* stream_title = p_info.meta_get("title", 0);
            if (stream_title != nullptr) {
                console::formatter() << "Radio stream updated title to: " << stream_title;
            }
            set_custom_now_playing_display("xd");*/
		}

        // --- REQUIRED STUBS (Missing these causes the error) ---
        void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override { (void)p_command; (void)p_paused; }
        void on_playback_seek(double p_time) override { (void)p_time; }
        void on_playback_pause(bool p_state) override { (void)p_state; }
        void on_playback_edited(metadb_handle_ptr p_track) override { (void)p_track; }
        void on_playback_dynamic_info(const file_info& p_info) override { (void)p_info; }
        void on_playback_time(double p_time) override { (void)p_time; }
        void on_volume_change(float p_new_val) override { (void)p_new_val; }

    private:
        static void refresh_metadata(metadb_handle_ptr p_track, const file_info& p_info, const foobar2000_io::t_filestats p_filestats)
        {
            if (p_track.is_valid()) {
                auto mio = metadb_io_v2::get();
                service_ptr_t<metadb_hint_list> hint_list = mio->create_hint_list();
                metadb_hint_list_v3::ptr hint_list_v3;
                hint_list_v3 ^= hint_list;
                hint_list_v3->add_hint_forced(p_track, p_info, p_filestats, true);
                fb2k::inMainThread([=] {hint_list->on_done(); });
            }
        }

        void set_custom_now_playing_display(const char* custom_text) {
            auto pc = playback_control::get();

            if (pc->is_playing()) {
                auto ui = ui_control::get();

                // 1. Create the status override object
                if (ui->override_status_text_create(override_obj)) {
                    // 2. Set your custom text
                    override_obj->override_text(custom_text);
                }

				metadb_handle_ptr now_playing;
                pfc::string8 formatted_output;
                if (pc->get_now_playing(now_playing)) {
                    // 3. Update the metadata for the currently playing track
                    file_info_impl info;
                    info.meta_set("TITLE", custom_text);
					refresh_metadata(now_playing, info, now_playing->get_filestats());
                }
            }
        }
        void check_stream(metadb_handle_ptr track) {
            if (track.is_empty()) return;

            const char* path = track->get_path();
            if (pfc::string_has_prefix(path, "http://") ||
                pfc::string_has_prefix(path, "https://") ||
                pfc::string_has_prefix(path, "icy://"))
            {
                console::formatter() << "[Radio Detected] Stream URL: " << path;
            }
            else {
                console::formatter() << "[Track Loaded] Not Radio: " << path;
            }

			if (is_kwsx_stream(track)) {
				console::formatter() << "[KWSX Stream Detected] URL: " << path;
				json data = get_stream_data();
				set_custom_now_playing_display(data["station"]["description"].get<std::string>().c_str());
			}
        }
     

        json get_stream_data() {
            auto client = http_client::get();
			http_request::ptr request = client->create_request("GET");
            request->add_header("User-Agent", "foo_kwsx/0.0.1");
            request->add_header("Accept", "application/json");

            // Execute request - returns a file::ptr pointing to the response stream
            // Throws exception_io on connection or non-2XX HTTP status failure
            file::ptr response_stream = request->run(NOW_PLAYING, fb2k::noAbort);

            pfc::string8 response_body;
            response_stream->read_string_raw(response_body, fb2k::noAbort);
            json data = json::parse(response_body.get_ptr());

            return data;
        }

		bool is_kwsx_stream(metadb_handle_ptr track) {
			if (track.is_empty()) return false;
			const char* path = track->get_path();
			if (strcmp(path, KWSX_URL) == 0) {
				return true;
			}
			return false;
		}
    };

    // Autoregisters this service with foobar2000's core engine
    FB2K_SERVICE_FACTORY(my_static_monitor);
}

