// SystemUSB - USB access for Digispark/UsbTiny.
// Linux/macOS: libusb-0.1 (compat).
// Windows: libusb-win32 (libusb0.dll) and/or libusb-1.0 (WinUSB via libusb-1.0.dll).


#ifdef windows

  // Minimal libusb-0.1 shaped API for DigiSpark.c / TinyispAccess.c.
  // Backed by libusb0.dll and/or libusb-1.0.dll at runtime.
  //
  // IMPORTANT: On Windows both libusb0 and libusb-1.0 export WINAPI (__stdcall).

  #define LIBUSB_PATH_MAX   512
  #define USB_TYPE_VENDOR   (0x02 << 5)
  #define USB_RECIP_DEVICE  0x00
  #define USB_ENDPOINT_IN   0x80
  #define USB_ENDPOINT_OUT  0x00

  #define USB_BACKEND_LIB0  1
  #define USB_BACKEND_LIB1  2

  struct usb_device_descriptor
  {
    unsigned char  bLength;
    unsigned char  bDescriptorType;
    unsigned short bcdUSB;
    unsigned char  bDeviceClass;
    unsigned char  bDeviceSubClass;
    unsigned char  bDeviceProtocol;
    unsigned char  bMaxPacketSize0;
    unsigned short idVendor;
    unsigned short idProduct;
    unsigned short bcdDevice;
    unsigned char  iManufacturer;
    unsigned char  iProduct;
    unsigned char  iSerialNumber;
    unsigned char  bNumConfigurations;
  };

  struct usb_device
  {
    struct usb_device             *next, *prev;
    char                           filename[LIBUSB_PATH_MAX];
    struct usb_bus                *bus;
    struct usb_device_descriptor   descriptor;
    void                          *config;
    void                          *dev;       /* native device ptr for backend */
    unsigned char                  devnum;    /* backend id: USB_BACKEND_LIB0/1 */
    unsigned char                  num_children;
    struct usb_device            **children;
  };

  struct usb_bus
  {
    struct usb_bus    *next, *prev;
    char               dirname[LIBUSB_PATH_MAX];
    struct usb_device *devices;
    unsigned long      location;
    struct usb_device *root_dev;
  };

  struct usb_dev_handle {
    int   backend;
    void *native;
  };
  typedef struct usb_dev_handle usb_dev_handle;

  // ---- libusb-0.1 (libusb-win32) ----
  static HINSTANCE lib0_mod = NULL;
  static void            (WINAPI *lib0_init)         (void) = NULL;
  static int             (WINAPI *lib0_find_busses)  (void) = NULL;
  static int             (WINAPI *lib0_find_devices) (void) = NULL;
  static struct usb_bus *(WINAPI *lib0_get_busses)   (void) = NULL;
  static void           *(WINAPI *lib0_open)         (void *dev) = NULL;
  static int             (WINAPI *lib0_close)        (void *dev) = NULL;
  static int             (WINAPI *lib0_control_msg)  (
                           void *dev,
                           int requesttype, int request,
                           int value, int index,
                           char *bytes, int size,
                           int timeout) = NULL;

  // ---- libusb-1.0 (WinUSB backend) ----
  // Packed like libusb.h on Windows (no extra padding between u8/u16 fields).
  #pragma pack(push, 1)
  struct libusb_device_descriptor {
    unsigned char  bLength;
    unsigned char  bDescriptorType;
    unsigned short bcdUSB;
    unsigned char  bDeviceClass;
    unsigned char  bDeviceSubClass;
    unsigned char  bDeviceProtocol;
    unsigned char  bMaxPacketSize0;
    unsigned short idVendor;
    unsigned short idProduct;
    unsigned short bcdDevice;
    unsigned char  iManufacturer;
    unsigned char  iProduct;
    unsigned char  iSerialNumber;
    unsigned char  bNumConfigurations;
  };
  #pragma pack(pop)

  static HINSTANCE lib1_mod = NULL;
  static void *lib1_ctx = NULL;
  static int   (WINAPI *lib1_init)(void **ctx) = NULL;
  static void  (WINAPI *lib1_exit)(void *ctx) = NULL;
  static intptr_t (WINAPI *lib1_get_device_list)(void *ctx, void ***list) = NULL;
  static void  (WINAPI *lib1_free_device_list)(void **list, int unref) = NULL;
  static int   (WINAPI *lib1_get_device_descriptor)(void *dev, struct libusb_device_descriptor *desc) = NULL;
  static int   (WINAPI *lib1_open)(void *dev, void **handle) = NULL;
  static void  (WINAPI *lib1_close)(void *handle) = NULL;
  static int   (WINAPI *lib1_claim_interface)(void *handle, int iface) = NULL;
  static int   (WINAPI *lib1_control_transfer)(
                 void *handle,
                 unsigned char request_type, unsigned char bRequest,
                 unsigned short wValue, unsigned short wIndex,
                 unsigned char *data, unsigned short wLength,
                 unsigned int timeout) = NULL;
  static void *(WINAPI *lib1_ref_device)(void *dev) = NULL;
  static void  (WINAPI *lib1_unref_device)(void *dev) = NULL;

  static struct usb_bus  win_bus;
  static struct usb_device *win_devices = NULL;
  static int usb_ready = 0;

  static void FreeWinDevices(void) {
    struct usb_device *d = win_devices;
    while (d) {
      struct usb_device *next = d->next;
      if (d->devnum == USB_BACKEND_LIB1 && d->dev && lib1_unref_device) {
        lib1_unref_device(d->dev);
      }
      Free(d);
      d = next;
    }
    win_devices = NULL;
    win_bus.devices = NULL;
  }

  static struct usb_device *AddDevice(int backend, void *native,
                                      unsigned short vid, unsigned short pid) {
    struct usb_device *d = Allocate(sizeof(*d));
    if (!d) return NULL;
    d->devnum = (unsigned char)backend;
    d->dev = native;
    d->descriptor.idVendor = vid;
    d->descriptor.idProduct = pid;
    d->next = win_devices;
    if (win_devices) win_devices->prev = d;
    win_devices = d;
    win_bus.devices = win_devices;
    return d;
  }

  static int LoadLibusb0(void) {
    if (lib0_mod) return 1;
    lib0_mod = LoadLibraryA("libusb0.dll");
    if (!lib0_mod) return 0;
    lib0_init         = (void *)GetProcAddress(lib0_mod, "usb_init");
    lib0_find_busses  = (void *)GetProcAddress(lib0_mod, "usb_find_busses");
    lib0_find_devices = (void *)GetProcAddress(lib0_mod, "usb_find_devices");
    lib0_get_busses   = (void *)GetProcAddress(lib0_mod, "usb_get_busses");
    lib0_open         = (void *)GetProcAddress(lib0_mod, "usb_open");
    lib0_close        = (void *)GetProcAddress(lib0_mod, "usb_close");
    lib0_control_msg  = (void *)GetProcAddress(lib0_mod, "usb_control_msg");
    if (!(lib0_init && lib0_find_busses && lib0_find_devices && lib0_get_busses
          && lib0_open && lib0_close && lib0_control_msg)) {
      FreeLibrary(lib0_mod); lib0_mod = NULL; return 0;
    }
    lib0_init();
    return 1;
  }

  static int LoadLibusb1(void) {
    if (lib1_mod) return 1;
    // Prefer DLL beside the exe (shipped for WinUSB), then PATH/System32.
    lib1_mod = LoadLibraryA("libusb-1.0.dll");
    if (!lib1_mod) return 0;
    lib1_init = (void *)GetProcAddress(lib1_mod, "libusb_init");
    lib1_exit = (void *)GetProcAddress(lib1_mod, "libusb_exit");
    lib1_get_device_list = (void *)GetProcAddress(lib1_mod, "libusb_get_device_list");
    lib1_free_device_list = (void *)GetProcAddress(lib1_mod, "libusb_free_device_list");
    lib1_get_device_descriptor = (void *)GetProcAddress(lib1_mod, "libusb_get_device_descriptor");
    lib1_open = (void *)GetProcAddress(lib1_mod, "libusb_open");
    lib1_close = (void *)GetProcAddress(lib1_mod, "libusb_close");
    lib1_claim_interface = (void *)GetProcAddress(lib1_mod, "libusb_claim_interface");
    lib1_control_transfer = (void *)GetProcAddress(lib1_mod, "libusb_control_transfer");
    lib1_ref_device = (void *)GetProcAddress(lib1_mod, "libusb_ref_device");
    lib1_unref_device = (void *)GetProcAddress(lib1_mod, "libusb_unref_device");
    if (!(lib1_init && lib1_get_device_list && lib1_free_device_list
          && lib1_get_device_descriptor && lib1_open && lib1_close
          && lib1_claim_interface && lib1_control_transfer
          && lib1_ref_device && lib1_unref_device)) {
      FreeLibrary(lib1_mod); lib1_mod = NULL; return 0;
    }
    if (lib1_init(&lib1_ctx) != 0) {
      FreeLibrary(lib1_mod); lib1_mod = NULL; lib1_ctx = NULL; return 0;
    }
    return 1;
  }

  static int usb_find_busses(void) { return 1; }

  static int usb_find_devices(void) {
    FreeWinDevices();
    win_bus.next = win_bus.prev = NULL;
    win_bus.devices = NULL;
    win_bus.location = 0;
    win_bus.root_dev = NULL;
    win_bus.dirname[0] = 'w';
    win_bus.dirname[1] = 0;

    // libusb-win32 driver path
    if (lib0_mod) {
      lib0_find_busses();
      lib0_find_devices();
      struct usb_bus *bus = lib0_get_busses();
      while (bus) {
        // libusb0 device nodes match the classic layout for descriptor + next.
        struct usb_device *src = bus->devices;
        while (src) {
          AddDevice(USB_BACKEND_LIB0, src,
                    src->descriptor.idVendor, src->descriptor.idProduct);
          src = src->next;
        }
        bus = bus->next;
      }
    }

    // WinUSB (and other libusb-1.0 Windows backends)
    if (lib1_mod && lib1_ctx) {
      void **list = NULL;
      intptr_t n = lib1_get_device_list(lib1_ctx, &list);
      if (n >= 0 && list) {
        for (intptr_t i = 0; i < n; i++) {
          struct libusb_device_descriptor desc;
          if (lib1_get_device_descriptor(list[i], &desc) != 0) continue;
          void *held = lib1_ref_device(list[i]);
          if (!held) continue;
          AddDevice(USB_BACKEND_LIB1, held, desc.idVendor, desc.idProduct);
        }
        lib1_free_device_list(list, 1);
      }
    }

    return 1;
  }

  static struct usb_bus *usb_get_busses(void) {
    return win_devices ? &win_bus : NULL;
  }

  static usb_dev_handle *usb_open(struct usb_device *dev) {
    if (!dev) return NULL;
    usb_dev_handle *h = Allocate(sizeof(*h));
    if (!h) return NULL;
    h->backend = dev->devnum;
    if (h->backend == USB_BACKEND_LIB0) {
      h->native = lib0_open(dev->dev);
      if (!h->native) { Free(h); return NULL; }
    } else if (h->backend == USB_BACKEND_LIB1) {
      void *lh = NULL;
      if (lib1_open(dev->dev, &lh) != 0 || !lh) { Free(h); return NULL; }
      // WinUSB requires a claimed interface for vendor control transfers.
      if (lib1_claim_interface(lh, 0) != 0) {
        lib1_close(lh);
        Free(h);
        return NULL;
      }
      h->native = lh;
    } else {
      Free(h); return NULL;
    }
    return h;
  }

  static int usb_close(usb_dev_handle *dev) {
    if (!dev) return 0;
    if (dev->backend == USB_BACKEND_LIB0 && lib0_close) {
      lib0_close(dev->native);
    } else if (dev->backend == USB_BACKEND_LIB1 && lib1_close) {
      lib1_close(dev->native);
    }
    Free(dev);
    return 0;
  }

  static int usb_control_msg(
      usb_dev_handle *dev,
      int requesttype, int request,
      int value, int index,
      char *bytes, int size,
      int timeout) {
    if (!dev || !dev->native) return -1;
    if (dev->backend == USB_BACKEND_LIB0) {
      return lib0_control_msg(dev->native, requesttype, request, value, index,
                              bytes, size, timeout);
    }
    if (dev->backend == USB_BACKEND_LIB1) {
      return lib1_control_transfer(
          dev->native,
          (unsigned char)requesttype, (unsigned char)request,
          (unsigned short)value, (unsigned short)index,
          (unsigned char *)bytes, (unsigned short)size,
          (unsigned int)timeout);
    }
    return -1;
  }

  int UsbInit(void) {
    if (usb_ready) return 1;
    int ok0 = LoadLibusb0();
    int ok1 = LoadLibusb1();
    if (!ok0 && !ok1) return 0;
    usb_ready = 1;
    return 1;
  }

  int UsbHasLibusb0(void) { return lib0_mod != NULL; }
  int UsbHasLibusb1(void) { return lib1_mod != NULL; }

#else

  // Linux / macOS - USB support via linked libusb-compat.
  #include <usb.h>

  // run the following command on mac to fix 'usb.h' file not found
  // brew install libusb libusb-compat

  int UsbInit(void) { usb_init(); return 1; }
  int UsbHasLibusb0(void) { return 1; }
  int UsbHasLibusb1(void) { return 0; }

#endif
