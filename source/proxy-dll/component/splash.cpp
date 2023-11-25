#include <std_include.hpp>
#include "splash.hpp"
#include "loader/component_loader.hpp"

#include "resource.hpp"

#include <utilities/nt.hpp>
#include <utilities/image.hpp>

namespace splash
{
	namespace
	{
		HWND window{};
		utilities::image::object image{};
		std::thread window_thread{};

		utilities::image::object load_splash_image()
		{
			//const auto self = utilities::nt::library::get_by_address(load_splash_image);
			//return LoadImageA(self, MAKEINTRESOURCE(IMAGE_SPLASH), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);

			const auto res = utilities::nt::load_resource(IMAGE_SPLASH);
			const auto img = utilities::image::load_image(res);
			return utilities::image::create_bitmap(img);
		}

		void enable_dpi_awareness()
		{
			const utilities::nt::library user32{ "user32.dll" };
			const auto set_dpi = user32
				? user32.get_proc<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
					"SetProcessDpiAwarenessContext")
				: nullptr;
			if (set_dpi)
			{
				set_dpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
			}
		}

		void destroy_window()
		{
			if (window && IsWindow(window))
			{
				ShowWindow(window, SW_HIDE);
				DestroyWindow(window);
				window = nullptr;

				if (window_thread.joinable())
				{
					window_thread.join();
				}

				window = nullptr;
			}
			else if (window_thread.joinable())
			{
				window_thread.detach();
			}
		}

		void show()
		{
			WNDCLASSA wnd_class;

			const auto self = utilities::nt::library::get_by_address(load_splash_image);

			wnd_class.style = CS_DROPSHADOW;
			wnd_class.cbClsExtra = 0;
			wnd_class.cbWndExtra = 0;
			wnd_class.lpszMenuName = nullptr;
			wnd_class.lpfnWndProc = DefWindowProcA;
			wnd_class.hInstance = self;
			wnd_class.hIcon = LoadIconA(self, MAKEINTRESOURCEA(ID_ICON));
			wnd_class.hCursor = LoadCursorA(nullptr, IDC_APPSTARTING);
			wnd_class.hbrBackground = reinterpret_cast<HBRUSH>(6);
			wnd_class.lpszClassName = "Black Ops 4 Splash Screen";

			if (RegisterClassA(&wnd_class))
			{
				const auto x_pixels = GetSystemMetrics(SM_CXFULLSCREEN);
				const auto y_pixels = GetSystemMetrics(SM_CYFULLSCREEN);

				if (image)
				{
					window = CreateWindowExA(WS_EX_APPWINDOW, "Black Ops 4 Splash Screen", "Project-Bo4",
						WS_POPUP | WS_SYSMENU,
						(x_pixels - 320) / 2, (y_pixels - 100) / 2, 320, 100, nullptr,
						nullptr,
						self, nullptr);

					if (window)
					{
						auto* const image_window = CreateWindowExA(0, "Static", nullptr, WS_CHILD | WS_VISIBLE | 0xEu,
							0, 0,
							320, 100, window, nullptr, self, nullptr);
						if (image_window)
						{
							RECT rect;
							SendMessageA(image_window, STM_SETIMAGE, IMAGE_BITMAP, image);
							GetWindowRect(image_window, &rect);

							const int width = rect.right - rect.left;
							rect.left = (x_pixels - width) / 2;

							const int height = rect.bottom - rect.top;
							rect.top = (y_pixels - height) / 2;

							rect.right = rect.left + width;
							rect.bottom = rect.top + height;
							AdjustWindowRect(&rect, WS_CHILD | WS_VISIBLE | 0xEu, 0);
							SetWindowPos(window, nullptr, rect.left, rect.top, rect.right - rect.left,
								rect.bottom - rect.top, SWP_NOZORDER);

							/*						SetWindowRgn(window,
														CreateRoundRectRgn(0, 0, rect.right - rect.left, rect.bottom - rect.top, 15,
															15), TRUE);*/

							ShowWindow(window, SW_SHOW);
							UpdateWindow(window);
						}
					}
				}
			}
		}

		bool draw_frame()
		{
			if (!window)
			{
				return false;
			}

			MSG msg{};
			bool success = true;

			while (PeekMessageW(&msg, nullptr, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);

				if (msg.message == WM_DESTROY && msg.hwnd == window)
				{
					PostQuitMessage(0);
				}

				if (msg.message == WM_QUIT)
				{
					success = false;
				}
			}
			return success;
		}

		void draw()
		{
			show();
			while (draw_frame())
			{
				std::this_thread::sleep_for(1ms);
			}

			window = nullptr;
			UnregisterClassA("Black Ops 4 Splash Screen", utilities::nt::library{});
		}
	}

	class component final : public component_interface
	{
	public:
		component()
		{
			enable_dpi_awareness();

			image = load_splash_image();
			window_thread = std::thread([this]
				{
					draw();
				});

			/*************************************************************************************************************
			** TODO : Again like many other components, splash needs complete revamp... 
			**
			**   *[X] Its crashing within unpack sequence because of unauthorized thread or window handle?
			**
			**   *[ ] There is significant interval between post_unpack and game window registeration. should replace
			**     dispose callback with something better like interception of CreateWindow to decide time to destroy window
			**
			**   *[X] enabling dpi_awareness interfering with protection system for some odd reason?
			*************************************************************************************************************/
		}

		void pre_destroy() override
		{
			destroy_window();
			if (window_thread.joinable())
			{
				window_thread.detach();
			}
		}

		void post_unpack() override
		{
			destroy_window();
		}
	};

	void hide()
	{
		if (window && IsWindow(window))
		{
			ShowWindow(window, SW_HIDE);
			UpdateWindow(window);
		}

		destroy_window();
	}

	HWND get_window()
	{
		return window;
	}
}

REGISTER_COMPONENT(splash::component)