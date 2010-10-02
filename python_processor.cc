#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Python.h"

#include <iostream>
#include <libecap/common/registry.h>
#include <libecap/common/errors.h>
#include <libecap/common/message.h>
#include <libecap/common/header.h>
#include <libecap/common/names.h>
#include <libecap/host/host.h>
#include <libecap/adapter/service.h>
#include <libecap/adapter/xaction.h>
#include <libecap/host/xaction.h>

namespace Adapter { // not required, but adds clarity

const char *script_source= "/home/knielsen/stuff.py";

using libecap::size_type;

class Service: public libecap::adapter::Service {
	public:
		// About
		virtual std::string uri() const; // unique across all vendors
		virtual std::string tag() const; // changes with version and config
		virtual void describe(std::ostream &os) const; // free-format info

		// Configuration
		virtual void configure(const Config &cfg);
		virtual void reconfigure(const Config &cfg);

		// Lifecycle
		virtual void start(); // expect makeXaction() calls
		virtual void stop(); // no more makeXaction() calls until start()
		virtual void retire(); // no more makeXaction() calls

		// Scope (XXX: this may be changed to look at the whole header)
		virtual bool wantsUrl(const char *url) const;

		// Work
		virtual libecap::adapter::Xaction *makeXaction(libecap::host::Xaction *hostx);

  Service()
    : main_module(0), main_dict(0), processor_class(0), last_access((time_t)-1)
  { }

  void reload_script(bool check_stat);

  PyObject *main_module;
  PyObject *main_dict;
  PyObject *processor_class;

  time_t last_access;
};


class Xaction: public libecap::adapter::Xaction {
	public:
  Xaction(libecap::host::Xaction *x, Service *parentService);
		virtual ~Xaction();

		// lifecycle
		virtual void start();
		virtual void stop();

        // adapted body transmission control
        virtual void abDiscard();
        virtual void abMake();
        virtual void abMakeMore();
        virtual void abStopMaking();

        // adapted body content extraction and consumption
        virtual libecap::Area abContent(size_type offset, size_type size);
        virtual void abContentShift(size_type size);

        // virgin body state notification
        virtual void noteVbContentDone(bool atEnd);
        virtual void noteVbContentAvailable();

		// libecap::Callable API, via libecap::host::Xaction
		virtual bool callable() const;

	protected:
		void adaptContent(std::string &chunk) const; // converts vb to ab
		libecap::host::Xaction *lastHostCall(); // clears hostx

	private:
		libecap::host::Xaction *hostx; // Host transaction rep

		std::string buffer; // for content adaptation

		typedef enum { opUndecided, opOn, opComplete, opNever } OperationState;
		OperationState receivingVb;
		OperationState sendingAb;

  Service *parent;
  PyObject *processor_object;
};

} // namespace Adapter

std::string Adapter::Service::uri() const {
	return "ecap://e-cap.org/ecap/services/sample/modifying";
}

std::string Adapter::Service::tag() const {
	return PACKAGE_VERSION;
}

void Adapter::Service::describe(std::ostream &os) const {
	os << "A modifying adapter from " << PACKAGE_NAME << " v" << PACKAGE_VERSION;
}

void Adapter::Service::configure(const Config &) {
	// this service is not configurable
}

void Adapter::Service::reconfigure(const Config &) {
	// this service is not configurable
}

void Adapter::Service::start() {
  // Don't call the parent class, as it asserts.
  // libecap::adapter::Service::start();

  Py_Initialize();

  main_module= PyImport_AddModule("__main__");
  if (!main_module)
  {
    fprintf(stderr, "Fatal: cannot obtain __main__ module\n");
    exit(1);
  }
  main_dict= PyModule_GetDict(main_module);
  if (!main_dict)
  {
    fprintf(stderr, "Fatal: cannot obtain __main__ dictionary\n");
    exit(1);
  }

  reload_script(false);
}

void
Adapter::Service::reload_script(bool check_stat)
{
  struct stat buf;
  if (stat(script_source, &buf))
  {
    fprintf(stderr, "Failed to stat(\"%s\"): %d\n", script_source, errno);
    return;
  }
  if (check_stat && buf.st_mtime == last_access)
      return;
  last_access= buf.st_mtime;

  FILE *fh = fopen(script_source, "r");
  if (!fh)
  {
    fprintf(stderr, "Fatal: cannot open file '%s'.\n", script_source);
    exit(1);
  }
  PyRun_File(fh, script_source, Py_file_input, main_dict, main_dict);
  fclose(fh);

  processor_class= PyDict_GetItemString(main_dict, "Processor");
  if (!processor_class)
  {
    fprintf(stderr, "Fatal: cannot obtain `Processor' class\n");
    exit(1);
  }
}

void Adapter::Service::stop() {
	// custom code would go here, but this service does not have one
  // libecap::adapter::Service::stop();
}

void Adapter::Service::retire() {
	// custom code would go here, but this service does not have one
  // libecap::adapter::Service::stop();

  Py_DECREF(processor_class);
  Py_DECREF(main_dict);

  Py_Finalize();
}

bool Adapter::Service::wantsUrl(const char *url) const {
	return true; // no-op is applied to all messages
}

libecap::adapter::Xaction *Adapter::Service::makeXaction(libecap::host::Xaction *hostx) {
  return new Adapter::Xaction(hostx, this);
}


Adapter::Xaction::Xaction(libecap::host::Xaction *x, Adapter::Service *parentService):
  hostx(x),
  receivingVb(opUndecided), sendingAb(opUndecided), parent(parentService),
  processor_object(0)
{
}

Adapter::Xaction::~Xaction() {
	if (libecap::host::Xaction *x = hostx) {
		hostx = 0;
		x->adaptationAborted();
	}
}

void Adapter::Xaction::start() {
	Must(hostx);
	if (hostx->virgin().body()) {
		receivingVb = opOn;
		hostx->vbMake(); // ask host to supply virgin body
	} else {
		receivingVb = opNever;
	}

        /* Initialise Python object to process content header/body. */
        parent->reload_script(true);

        processor_object= PyInstance_New(parent->processor_class, NULL, NULL);
        if (!processor_object)
          fprintf(stderr, "Error: failed to instantiate Python `Processor' object.\n");

	/* adapt message header */

	libecap::shared_ptr<libecap::Message> adapted = hostx->virgin().clone();
    Must(adapted != 0);

	// delete ContentLength header because we may change the length
	// unknown length may have performance implications for the host
	adapted->header().removeAny(libecap::headerContentLength);

#if 0
	// add a custom header
	static const libecap::Name name("X-Ecap");
	const libecap::Header::Value value =
		libecap::Area::FromTempString(libecap::MyHost().uri());
	adapted->header().add(name, value);
#endif

	if (!adapted->body()) {
		sendingAb = opNever; // there is nothing to send
		lastHostCall()->useAdapted(adapted);
	} else {
		hostx->useAdapted(adapted);
	}
}

void Adapter::Xaction::stop() {
  Py_DECREF(processor_object);
	hostx = 0;
    // the caller will delete
}

void Adapter::Xaction::abDiscard()
{
	Must(sendingAb == opUndecided); // have not started yet
	sendingAb = opNever;
	if (receivingVb == opOn)
		hostx->vbDiscard(); // we are not interested in vb then
}

void Adapter::Xaction::abMake()
{
	Must(sendingAb == opUndecided); // have not yet started or decided not to send
	Must(hostx->virgin().body()); // that is our only source of ab content

    // we are or were receiving vb
	Must(receivingVb == opOn || receivingVb == opComplete);
	
	sendingAb = opOn;
	if (!buffer.empty())
		hostx->noteAbContentAvailable();
}

void Adapter::Xaction::abMakeMore()
{
	Must(receivingVb == opOn); // a precondition for receiving more vb
	hostx->vbMakeMore();
}

void Adapter::Xaction::abStopMaking()
{
	sendingAb = opComplete;
	if (receivingVb == opOn)
		hostx->vbDiscard(); // we are not interested in vb then
}


libecap::Area Adapter::Xaction::abContent(size_type offset, size_type size) {
	Must(sendingAb == opOn);
	return libecap::Area::FromTempString(buffer.substr(offset, size));
}

void Adapter::Xaction::abContentShift(size_type size) {
	Must(sendingAb == opOn);
	buffer.erase(0, size);
}

void Adapter::Xaction::noteVbContentDone(bool atEnd)
{
	Must(receivingVb == opOn);
	receivingVb = opComplete;
	if (sendingAb == opOn) {
		hostx->noteAbContentDone(atEnd);
		sendingAb = opComplete;
	}
}

void Adapter::Xaction::noteVbContentAvailable()
{
	Must(receivingVb == opOn);

	const libecap::Area vb = hostx->vbContent(0, libecap::nsize); // get all vb
	std::string chunk = vb.toString(); // expensive, but simple
	adaptContent(chunk);
	buffer += chunk; // buffer what we got

	if (sendingAb == opOn)
		hostx->noteAbContentAvailable();
}

void Adapter::Xaction::adaptContent(std::string &chunk) const {
	// this is oversimplified; production code should worry about content
	// split by arbitrary chunk boundaries, efficiency, and other things

	// another simplification: victim does not belong to replacement


  if (processor_object)
  {
    PyObject *result= PyObject_CallMethod(processor_object, "process", "s",
                                          chunk.c_str());
    if (result)
    {
      const char *output= PyString_AsString(result);
      if (output)
      {
        chunk= output;
      }
      Py_DECREF(result);
    }
  }
}

bool Adapter::Xaction::callable() const {
    return hostx != 0; // no point to call us if we are done
}

// this method is used to make the last call to hostx transaction
// last call may delete adapter transaction if the host no longer needs it
// TODO: replace with hostx-independent "done" method
libecap::host::Xaction *Adapter::Xaction::lastHostCall() {
	libecap::host::Xaction *x = hostx;
	Must(x);
	hostx = 0;
	return x;
}

// create the adapter and register with libecap to reach the host application
static const bool Registered = (libecap::RegisterService(new Adapter::Service), true);
