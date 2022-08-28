// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// Defines Message, the abstract interface implemented by non-lite
// protocol message objects.  Although it's possible to implement this
// interface manually, most users will use the protocol compiler to
// generate implementations.
//
// Example usage:
//
// Say you have a message defined as:
//
//   message Foo {
//     optional string text = 1;
//     repeated int32 numbers = 2;
//   }
//
// Then, if you used the protocol compiler to generate a class from the above
// definition, you could use it like so:
//
//   string data;  // Will store a serialized version of the message.
//
//   {
//     // Create a message and serialize it.
//     Foo foo;
//     foo.set_text("Hello World!");
//     foo.add_numbers(1);
//     foo.add_numbers(5);
//     foo.add_numbers(42);
//
//     foo.SerializeToString(&data);
//   }
//
//   {
//     // Parse the serialized message and check that it contains the
//     // correct data.
//     Foo foo;
//     foo.ParseFromString(data);
//
//     assert(foo.text() == "Hello World!");
//     assert(foo.numbers_size() == 3);
//     assert(foo.numbers(0) == 1);
//     assert(foo.numbers(1) == 5);
//     assert(foo.numbers(2) == 42);
//   }
//
//   {
//     // Same as the last block, but do it dynamically via the Message
//     // reflection interface.
//     Message* foo = new Foo;
//     const Descriptor* descriptor = foo->GetDescriptor();
//
//     // Get the descriptors for the fields we're interested in and verify
//     // their types.
//     const FieldDescriptor* text_field = descriptor->FindFieldByName("text");
//     assert(text_field != NULL);
//     assert(text_field->type() == FieldDescriptor::TYPE_STRING);
//     assert(text_field->label() == FieldDescriptor::LABEL_OPTIONAL);
//     const FieldDescriptor* numbers_field = descriptor->
//                                            FindFieldByName("numbers");
//     assert(numbers_field != NULL);
//     assert(numbers_field->type() == FieldDescriptor::TYPE_INT32);
//     assert(numbers_field->label() == FieldDescriptor::LABEL_REPEATED);
//
//     // Parse the message.
//     foo->ParseFromString(data);
//
//     // Use the reflection interface to examine the contents.
//     const Reflection* reflection = foo->GetReflection();
//     assert(reflection->GetString(foo, text_field) == "Hello World!");
//     assert(reflection->FieldSize(foo, numbers_field) == 3);
//     assert(reflection->GetRepeatedInt32(foo, numbers_field, 0) == 1);
//     assert(reflection->GetRepeatedInt32(foo, numbers_field, 1) == 5);
//     assert(reflection->GetRepeatedInt32(foo, numbers_field, 2) == 42);
//
//     delete foo;
//   }

#ifndef GOOGLE_PROTOBUF_MESSAGE_H__
#define GOOGLE_PROTOBUF_MESSAGE_H__

#include <iosfwd>
#include <string>
#include <vector>

#include <google/protobuf/message_lite.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>


#define GOOGLE_PROTOBUF_HAS_ONEOF

namespace google {
namespace protobuf {

// Defined in this file.
class Message;
class Reflection;
class MessageFactory;

// Defined in other files.
class UnknownFieldSet;         // unknown_field_set.h
namespace io {
  class ZeroCopyInputStream;   // zero_copy_stream.h
  class ZeroCopyOutputStream;  // zero_copy_stream.h
  class CodedInputStream;      // coded_stream.h
  class CodedOutputStream;     // coded_stream.h
}


template<typename T>
class RepeatedField;     // repeated_field.h

template<typename T>
class RepeatedPtrField;  // repeated_field.h

// A container to hold message metadata.
struct Metadata {
  const Descriptor* descriptor;
  const Reflection* reflection;
};

// Abstract interface for protocol messages.
//
// See also MessageLite, which contains most every-day operations.  Message
// adds descriptors and reflection on top of that.
//
// The methods of this class that are virtual but not pure-virtual have
// default implementations based on reflection.  Message classes which are
// optimized for speed will want to override these with faster implementations,
// but classes optimized for code size may be happy with keeping them.  See
// the optimize_for option in descriptor.proto.
class LIBPROTOBUF_EXPORT Message : public MessageLite {
 public:
  inline Message() {}
  virtual ~Message();

  // Basic Operations ------------------------------------------------

  // Construct a new instance of the same type.  Ownership is passed to the
  // caller.  (This is also defined in MessageLite, but is defined again here
  // for return-type covariance.)
  virtual Message* New() const = 0;

  // Make this message into a copy of the given message.  The given message
  // must have the same descriptor, but need not necessarily be the same class.
  // By default this is just implemented as "Clear(); MergeFrom(from);".
  virtual void CopyFrom(const Message& from);

  // Merge the fields from the given message into this message.  Singular
  // fields will be overwritten, if specified in from, except for embedded
  // messages which will be merged.  Repeated fields will be concatenated.
  // The given message must be of the same type as this message (i.e. the
  // exact same class).
  virtual void MergeFrom(const Message& from);

  // Verifies that IsInitialized() returns true.  GOOGLE_CHECK-fails otherwise, with
  // a nice error message.
  void CheckInitialized() const;

  // Slowly build a list of all required fields that are not set.
  // This is much, much slower than IsInitialized() as it is implemented
  // purely via reflection.  Generally, you should not call this unless you
  // have already determined that an error exists by calling IsInitialized().
  void FindInitializationErrors(vector<string>* errors) const;

  // Like FindInitializationErrors, but joins all the strings, delimited by
  // commas, and returns them.
  string InitializationErrorString() const;

  // Clears all unknown fields from this message and all embedded messages.
  // Normally, if unknown tag numbers are encountered when parsing a message,
  // the tag and value are stored in the message's UnknownFieldSet and
  // then written back out when the message is serialized.  This allows servers
  // which simply route messages to other servers to pass through messages
  // that have new field definitions which they don't yet know about.  However,
  // this behavior can have security implications.  To avoid it, call this
  // method after parsing.
  //
  // See Reflection::GetUnknownFields() for more on unknown fields.
  virtual void DiscardUnknownFields();

  // Computes (an estimate of) the total number of bytes currently used for
  // storing the message in memory.  The default implementation calls the
  // Reflection object's SpaceUsed() method.
  virtual int SpaceUsed() const;

  // Debugging & Testing----------------------------------------------

  // Generates a human readable form of this message, useful for debugging
  // and other purposes.
  string DebugString() const;
  // Like DebugString(), but with less whitespace.
  string ShortDebugString() const;
  // Like DebugString(), but do not escape UTF-8 byte sequences.
  string Utf8DebugString() const;
  // Convenience function useful in GDB.  Prints DebugString() to stdout.
  void PrintDebugString() const;

  // Heavy I/O -------------------------------------------------------
  // Additional parsing and serialization methods not implemented by
  // MessageLite because they are not supported by the lite library.

  // Parse a protocol buffer from a file descriptor.  If successful, the entire
  // input will be consumed.
  bool ParseFromFileDescriptor(int file_descriptor);
  // Like ParseFromFileDescriptor(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromFileDescriptor(int file_descriptor);
  // Parse a protocol buffer from a C++ istream.  If successful, the entire
  // input will be consumed.
  bool ParseFromIstream(istream* input);
  // Like ParseFromIstream(), but accepts messages that are missing
  // required fields.
  bool ParsePartialFromIstream(istream* input);

  // Serialize the message and write it to the given file descriptor.  All
  // required fields must be set.
  bool SerializeToFileDescriptor(int file_descriptor) const;
  // Like SerializeToFileDescriptor(), but allows missing required fields.
  bool SerializePartialToFileDescriptor(int file_descriptor) const;
  // Serialize the message and write it to the given C++ ostream.  All
  // required fields must be set.
  bool SerializeToOstream(ostream* output) const;
  // Like SerializeToOstream(), but allows missing required fields.
  bool SerializePartialToOstream(ostream* output) const;


  // Reflection-based methods ----------------------------------------
  // These methods are pure-virtual in MessageLite, but Message provides
  // reflection-based default implementations.

  virtual string GetTypeName() const;
  virtual void Clear();
  virtual bool IsInitialized() const;
  virtual void CheckTypeAndMergeFrom(const MessageLite& other);
  virtual bool MergePartialFromCodedStream(io::CodedInputStream* input);
  virtual int ByteSize() const;
  virtual void SerializeWithCachedSizes(io::CodedOutputStream* output) const;

 private:
  // This is called only by the default implementation of ByteSize(), to
  // update the cached size.  If you override ByteSize(), you do not need
  // to override this.  If you do not override ByteSize(), you MUST override
  // this; the default implementation will crash.
  //
  // The method is private because subclasses should never call it; only
  // override it.  Yes, C++ lets you do that.  Crazy, huh?
  virtual void SetCachedSize(int size) const;

 public:

  // Introspection ---------------------------------------------------

  // Typedef for backwards-compatibility.
  typedef google::protobuf::Reflection Reflection;

  // Get a Descriptor for this message's type.  This describes what
  // fields the message contains, the types of those fields, etc.
  const Descriptor* GetDescriptor() const { return GetMetadata().descriptor; }

  // Get the Reflection interface for this Message, which can be used to
  // read and modify the fields of the Message dynamically (in other words,
  // without knowing the message type at compile time).  This object remains
  // property of the Message.
  //
  // This method remains virtual in case a subclass does not implement
  // reflection and wants to override the default behavior.
  virtual const Reflection* GetReflection() const {
    return GetMetadata().reflection;
  }

 protected:
  // Get a struct containing the metadata for the Message. Most subclasses only
  // need to implement this method, rather than the GetDescriptor() and
  // GetReflection() wrappers.
  virtual Metadata GetMetadata() const  = 0;


 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Message);
};

// This interface contains methods that can be used to dynamically access
// and modify the fields of a protocol message.  Their semantics are
// similar to the accessors the protocol compiler generates.
//
// To get the Reflection for a given Message, call Message::GetReflection().
//
// This interface is separate from Message only for efficiency reasons;
// the vast majority of implementations of Message will share the same
// implementation of Reflection (GeneratedMessageReflection,
// defined in generated_message.h), and all Messages of a particular class
// should share the same Reflection object (though you should not rely on
// the latter fact).
//
// There are several ways that these methods can be used incorrectly.  For
// example, any of the following conditions will lead to undefined
// results (probably assertion failures):
// - The FieldDescriptor is not a field of this message type.
// - The method called is not appropriate for the field's type.  For
//   each field type in FieldDescriptor::TYPE_*, there is only one
//   Get*() method, one Set*() method, and one Add*() method that is
//   valid for that type.  It should be obvious which (except maybe
//   for TYPE_BYTES, which are represented using strings in C++).
// - A Get*() or Set*() method for singular fields is called on a repeated
//   field.
// - GetRepeated*(), SetRepeated*(), or Add*() is called on a non-repeated
//   field.
// - The Message object passed to any method is not of the right type for
//   this Reflection object (i.e. message.GetReflection() != reflection).
//
// You might wonder why there is not any abstract representation for a field
// of arbitrary type.  E.g., why isn't there just a "GetField()" method that
// returns "const Field&", where "Field" is some class with accessors like
// "GetInt32Value()".  The problem is that someone would have to deal with
// allocating these Field objects.  For generated message classes, having to
// allocate space for an additional object to wrap every field would at least
// double the message's memory footprint, probably worse.  Allocating the
// objects on-demand, on the other hand, would be expensive and prone to
// memory leaks.  So, instead we ended up with this flat interface.
//
// TODO(kenton):  Create a utility class which callers can use to read and
//   write fields from a Reflection without paying attention to the type.
class LIBPROTOBUF_EXPORT Reflection {
 public:
  inline Reflection() {}
  virtual ~Reflection();

  // Get the UnknownFieldSet for the message.  This contains fields which
  // were seen when the Message was parsed but were not recognized according
  // to the Message's definition.
  virtual const UnknownFieldSet& GetUnknownFields(
      const Message& message) const = 0;
  // Get a mutable pointer to the UnknownFieldSet for the message.  This
  // contains fields which were seen when the Message was parsed but were not
  // recognized according to the Message's definition.
  virtual UnknownFieldSet* MutableUnknownFields(Message* message) const = 0;

  // Estimate the amount of memory used by the message object.
  virtual int SpaceUsed(const Message& message) const = 0;

  // Check if the given non-repeated field is set.
  virtual bool HasField(const Message& message,
                        const FieldDescriptor* field) const = 0;

  // Get the number of elements of a repeated field.
  virtual int FieldSize(const Message& message,
                        const FieldDescriptor* field) const = 0;

  // Clear the value of a field, so that HasField() returns false or
  // FieldSize() returns zero.
  virtual void ClearField(Message* message,
                          const FieldDescriptor* field) const = 0;

  // Check if the oneof is set. Returns ture if any field in oneof
  // is set, false otherwise.
  // TODO(jieluo) - make it pure virtual after updating all
  // the subclasses.
  virtual bool HasOneof(const Message& message,
                        const OneofDescriptor* oneof_descriptor) const {
    return false;
  }

  virtual void ClearOneof(Message* message,
                          const OneofDescriptor* oneof_descriptor) const {}

  // Returns the field descriptor if the oneof is set. NULL otherwise.
  // TODO(jieluo) - make it pure virtual.
  virtual const FieldDescriptor* GetOneofFieldDescriptor(
      const Message& message,
      const OneofDescriptor* oneof_descriptor) const {
    return NULL;
  }

  // Removes the last element of a repeated field.
  // We don't provide a way to remove any element other than the last
  // because it invites inefficient use, such as O(n^2) filtering loops
  // that should have been O(n).  If you want to remove an element other
  // than the last, the best way to do it is to re-arrange the elements
  // (using Swap()) so that the one you want removed is at the end, then
  // call RemoveLast().
  virtual void RemoveLast(Message* message,
                          const FieldDescriptor* field) const = 0;
  // Removes the last element of a repeated message field, and returns the
  // pointer to the caller.  Caller takes ownership of the returned pointer.
  virtual Message* ReleaseLast(Message* message,
                               const FieldDescriptor* field) const = 0;

  // Swap the complete contents of two messages.
  virtual void Swap(Message* message1, Message* message2) const = 0;

  // Swap fields listed in fields vector of two messages.
  virtual void SwapFields(Message* message1,
                          Message* message2,
                          const vector<const FieldDescriptor*>& fields)
      const = 0;

  // Swap two elements of a repeated field.
  virtual void SwapElements(Message* message,
                            const FieldDescriptor* field,
                            int index1,
                            int index2) const = 0;

  // List all fields of the message which are currently set.  This includes
  // extensions.  Singular fields will only be listed if HasField(field) would
  // return true and repeated fields will only be listed if FieldSize(field)
  // would return non-zero.  Fields (both normal fields and extension fields)
  // will be listed ordered by field number.
  virtual void ListFields(const Message& message,
                          vector<const FieldDescriptor*>* output) const = 0;

  // Singular field getters ------------------------------------------
  // These get the value of a non-repeated field.  They return the default
  // value for fields that aren't set.

  virtual int32  GetInt32 (const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual int64  GetInt64 (const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual uint32 GetUInt32(const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual uint64 GetUInt64(const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual float  GetFloat (const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual double GetDouble(const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual bool   GetBool  (const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual string GetString(const Message& message,
                           const FieldDescriptor* field) const = 0;
  virtual const EnumValueDescriptor* GetEnum(
      const Message& message, const FieldDescriptor* field) const = 0;
  // See MutableMessage() for the meaning of the "factory" parameter.
  virtual const Message& GetMessage(const Message& message,
                                    const FieldDescriptor* field,
                                    MessageFactory* factory = NULL) const = 0;

  // Get a string value without copying, if possible.
  //
  // GetString() necessarily returns a copy of the string.  This can be
  // inefficient when the string is already stored in a string object in the
  // underlying message.  GetStringReference() will return a reference to the
  // underlying string in this case.  Otherwise, it will copy the string into
  // *scratch and return that.
  //
  // Note:  It is perfectly reasonable and useful to write code like:
  //     str = reflection->GetStringReference(field, &str);
  //   This line would ensure that only one copy of the string is made
  //   regardless of the field's underlying representation.  When initializing
  //   a newly-constructed string, though, it's just as fast and more readable
  //   to use code like:
  //     string str = reflection->GetString(field);
  virtual const string& GetStringReference(const Message& message,
                                           const FieldDescriptor* field,
                                           string* scratch) const = 0;


  // Singular field mutators -----------------------------------------
  // These mutate the value of a non-repeated field.

  virtual void SetInt32 (Message* message,
                         const FieldDescriptor* field, int32  value) const = 0;
  virtual void SetInt64 (Message* message,
                         const FieldDescriptor* field, int64  value) const = 0;
  virtual void SetUInt32(Message* message,
                         const FieldDescriptor* field, uint32 value) const = 0;
  virtual void SetUInt64(Message* message,
                         const FieldDescriptor* field, uint64 value) const = 0;
  virtual void SetFloat (Message* message,
                         const FieldDescriptor* field, float  value) const = 0;
  virtual void SetDouble(Message* message,
                         const FieldDescriptor* field, double value) const = 0;
  virtual void SetBool  (Message* message,
                         const FieldDescriptor* field, bool   value) const = 0;
  virtual void SetString(Message* message,
                         const FieldDescriptor* field,
                         const string& value) const = 0;
  virtual void SetEnum  (Message* message,
                         const FieldDescriptor* field,
                         const EnumValueDescriptor* value) const = 0;
  // Get a mutable pointer to a field with a message type.  If a MessageFactory
  // is provided, it will be used to construct instances of the sub-message;
  // otherwise, the default factory is used.  If the field is an extension that
  // does not live in the same pool as the containing message's descriptor (e.g.
  // it lives in an overlay pool), then a MessageFactory must be provided.
  // If you have no idea what that meant, then you probably don't need to worry
  // about it (don't provide a MessageFactory).  WARNING:  If the
  // FieldDescriptor is for a compiled-in extension, then
  // factory->GetPrototype(field->message_type() MUST return an instance of the
  // compiled-in class for this type, NOT DynamicMessage.
  virtual Message* MutableMessage(Message* message,
                                  const FieldDescriptor* field,
                                  MessageFactory* factory = NULL) const = 0;
  // Replaces the message specified by 'field' with the already-allocated object
  // sub_message, passing ownership to the message.  If the field contained a
  // message, that message is deleted.  If sub_message is NULL, the field is
  // cleared.
  virtual void SetAllocatedMessage(Message* message,
                                   Message* sub_message,
                                   const FieldDescriptor* field) const = 0;
  // Releases the message specified by 'field' and returns the pointer,
  // ReleaseMessage() will return the message the message object if it exists.
  // Otherwise, it may or may not return NULL.  In any case, if the return value
  // is non-NULL, the caller takes ownership of the pointer.
  // If the field existed (HasField() 1 2 , i , 7 7 4 7 6 3 2 9 1 5 3 2 8 8 2 4 7 2 9 , 1 4 2 8 0 9 2 7 6 8 4 0 0 1 8 8 7 6 6 3 , 1 3 1 0 7 2   / p r e f e t c h : 1         À6%òp?A    ^>A    Ø2  l    d1  °  ğ  t  04  Ü  È0  D   ì#  P"  h"  H   4  	   ¢    ÷   ! J å³È  ¦  C	 ¥  c  "   «   !  Š ;  %  ‚ Á  ¢  a S  "  ¢yš–{  õ8 #  ÷  ¿  ø  “  ™" g   
\  &b Ï   
\ 3  ›" ¯   œ" £   " '  " >¢Ìä D -  Àò  °Ï?A     ·’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 1 b   m 3 2  Àh è3  œ%   à?A            €SÛÚ«ÿÿpQx%†¼ÿÿD3      $       Ğ      0                          À8  è3  œ%  à?A            €SÛÚ«ÿÿpQx%†¼ÿÿ       ÀD 
§á?A        C          Pà×    à»Ó«ÿÿ`%%Ü«ÿÿÍ      |2  K € Àh è3  œ%  Hâ?A                    p[%†¼ÿÿ «                                                   Àò  ¥ø?A     §’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 1 d     C   Àh è3  œ%  @A             ³áÙ«ÿÿp!†¼ÿÿøŸ      $       `                                 À8  è3  œ%  @A             ³áÙ«ÿÿp!†¼ÿÿ       Àh è3  œ%  Æ@A                     ·|%†¼ÿÿ 
                                                  Àò  1'@A     —’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 1 f   Ó«ÿÿ Àò  Å2@A    p‘’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 0   À% ÀD 
>@A        C          €DØ    à»Ó«ÿÿ`%%Ü«ÿÿø      |2      Àò  F@@A     ‡’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 1   À(  ÀD 
™M@A        C          EØ    à»Ó«ÿÿ`%%Ü«ÿÿô      |2      Àò  ®O@A    p’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 2   c e \  ÀD 
O]@A        C           IØ    à»Ó«ÿÿ`%%Ü«ÿÿ      |2  e r  Àò  O_@A     w’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 3   Ü4     Àò  Zi@A    pq’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 4   ‡6+ Àò  Ñr@A     g’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 5   s h e  ÀD 
S~@A        C          @KØ    à»Ó«ÿÿ`%%Ü«ÿÿ}      |2       Àò  _€@A    pa’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 6   D e v  Àò  \Š@A     W’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 7   i n d  Àò  ’@A    pQ’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 8   d l l  Àò  á™@A     I’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 9   _ü   ÀD 
¨@A        C          °UØ    à»Ó«ÿÿ`%%Ü«ÿÿ¶      |2  r d  Àò  Sª@A     +’%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 a   y s t  ÀD 
»@A        C          àUØ    à»Ó«ÿÿ`%%Ü«ÿÿ–      |2  ü   Àò  úÍ@A     7”%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 c     ¨#   ÀD 
6Û@A        C          PVØ    à»Ó«ÿÿ`%%Ü«ÿÿ      |2  fÒd Àò  …İ@A    p1”%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 d   zÒ   ÀD 
Ğë@A        C          `WØ    à»Ó«ÿÿ`%%Ü«ÿÿn      |2  u
 Àò  ©î@A     '”%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 2 e          ÀD 
4û@A        C          ÀWØ    à»Ó«ÿÿ`%%Ü«ÿÿÕ      |2  e \  Àò  ÆAA     ”%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 0   w s \  ÀD 
şAA        C           XØ    à»Ó«ÿÿ`%%Ü«ÿÿÊ      |2       Àò  qAA    p”%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 1   ˆ`ü   ÀD 
í.AA        C          p]Ø    à»Ó«ÿÿ`%%Ü«ÿÿs      |2  n d  Àò  u1AA     ”%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 2   d l l  À8  è3  œ%  ¹=AA            @Í!Ú«ÿÿ                ÀD 
CAA        C          ÀaØ    à»Ó«ÿÿ`%%Ü«ÿÿ      |2  s k  ÀØ%ÕDAA    ëp?A    à$  8  d1  4  ø  ,  p  ¨#  |  @3  |2  è3  %  ø2    (     " FŒø [<  "  Šb ;  "  Š3 C  "  Š Ãi   "  ÷-  ó$ RJ Nœ Ë  ”" —  %& ÒF w	    R+ Ns Ò Û   %& "R% N_ ã  "  V 1 +  "  ŠY ó  ¢ fÒ '+  "  ŠÏ   "  j Ä[  "  n=³  "  n,2Ò&	jÒŠÊq o  š¦ ¶Œ4Ï  "  ŠI £  "  n…{   "  ŠÊ,    "  Š  jR( ¶Œâ Ó  "  :á1  "  j uË  "  Š§ §  "  j€¢÷  "  Š ï
  "  j€Ú«  "  Šl   "  Š( §  "  jR.>g j€
ã  ¢  ŠŠ g  "  j ê—  "  Š„ ë  "  j ‘¿  "  ŠS _
  "  Šô 2Òéj 02Ò¦j z2ÒÔj #2Ò± Š5÷  &  ŠH C  &  ŠT «  &  Š7   &  Š3 Ç  &  Š3 7  &  Š* ›   &  Š“2Òlj€Ë2Roj€52ÒAj kn ` Àò  CGAA    p”%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 3    :  ÀD 
AJAA                 0Í	   p[%†¼ÿÿP€}Ú«ÿÿ      `   ¤  ÀD 
KAA        C          ,Ş    à»Ó«ÿÿàÛ«ÿÿk	      è3  ü   Àò  TAA    pñ“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 4   w i n  ÀD 
0dAA        C          €fØ    à»Ó«ÿÿàÛ«ÿÿ.      |2  ”&   Àò  ¢fAA     ç“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 5   ’    ÀD 
¨wAA        C          `iØ    à»Ó«ÿÿ Ù˜Ú«ÿÿ´      |2      Àò  )AA     ×“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 7     ²  ÀD 
r AA        C          kØ    à»Ó«ÿÿ Ù˜Ú«ÿÿ5      |2  e v  Àj H4  ¨*  "¡AA            ¨*  H4   €U«Œıÿÿ U«Œıÿÿ  HNk    àGNk          ğÅhFü   àMk          ’    Àò  V»AA     Ç“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 9          Àü 
D4  @4  
ÉAA              –÷   7     @4  )M6 ¦0…b      –÷                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ P r o g r a m   F i l e s   ( x 8 6 ) \ M i c r o s o f t \ E d g e \ A p p l i c a t i o n \ m s e d g e . e x e       À¾ 
D4  @4  ¿ÊAA              çaü   P     @4  ‹ã ìT{      çaü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ n t d l l . d l l      ÀD 
åÍAA        C          °O×    à»Ó«ÿÿ Ù˜Ú«ÿÿõ      |2       Àò  zĞAA    pÁ“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 a   #†¼ÿÿ À%/åAA    ÏDAA    `   |2  %  ô  (  è3  ô*  H4  ¼  l  p3  „+  D4  ¬3  l  d	      %  F ¹
ÒW0  $ ç  ó  ÒC ç  €$  Vm
Ò²	c:  $ Ò7 _  —& b 0 ^Ø¿ &" 
Òz
Ÿ9  $ g  
  —    2ÒÛ6  $ C  ı  ë  ş  >Ò=[  ù0  S  ÿ   ÀD 
«æAA        C          ``Ö    à»Ó«ÿÿ Ù˜Ú«ÿÿä      |2    Àò  `éAA     ·“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 b   èÙ«ÿÿ ÀD 
DBA        C          °(×    à»Ó«ÿÿ Ù˜Ú«ÿÿ¼      |2  «ÿÿ Àò  ÎBA    p±“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 c   \ z 9  Àò  5BA     §“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 d   5 9    ÀD 
sBA        C          ğb×    à»Ó«ÿÿ Ù˜Ú«ÿÿ      |2  ü   Àò  àBA    p¡“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 e   ;"  ÀD 
'/BA        C          0‡×    à»Ó«ÿÿ Ù˜Ú«ÿÿ      |2  k   Àò  †1BA     —“%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 3 f   ~`ü   À 
D4  @4  <xBA              ¤Vü   €     @4  ”g ¦0…b      ¤Vü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ P r o g r a m   F i l e s   ( x 8 6 ) \ M i c r o s o f t \ E d g e \ A p p l i c a t i o n \ 1 0 1 . 0 . 1 2 1 0 . 5 3 \ m s e d g e _ e l f . d l l    ÀÄ 
D4  @4  O®BA              @-Ó   à
     @4  ?Ò
 4e8A    @-Ó                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ a d v a p i 3 2 . d l l   @&   ÀÀ 
D4  @4  ó¸BA             aü   à	     @4  ]è	 9ŸOV    aü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ m s v c r t . d l l    ÀÂ 
D4  @4  îÁBA             Xaü   À	     @4  Ãë	 !tÕS    Xaü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ s e c h o s t . d l l   Ô!†¼ÿÿ ÀÀ 
D4  @4  ËBA             ì_ü   P     @4  Ì îüZ±    ì_ü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ r p c r t 4 . d l l    ÀÆ 
D4  @4  ~)CA             Ú^ü   À      @4  J Cšè(      Ú^ü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ c r y p t b a s e . d l l      ÀÔ 
D4  @4  8CA             ã_ü         @4  şÖ ÔÓé    ã_ü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ b c r y p t p r i m i t i v e s . d l l   y \  Àò  5bCA     ‹%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 4 e   b i n  ÀD 
tCA        C          Ü    à»Ó«ÿÿ Ù˜Ú«ÿÿ9      |2       Àò  ”CA     {%†¼ÿÿ\ D e v i c e \ H a r d d i s k V o l u m e 2 \ U s e r s \ z 9 y a y \ A p p D a t a \ L o c a l \ M i c r o s o f t \ E d g e \ U s e r   D a t a \ D e f a u l t \ C a c h e \ C a c h e _ D a t a \ f _ 0 0 2 e 4 f   ë4     ÀÀ 
(4  $4  —ÄCA             ¾aü   Ğ
     $4  Zò
 ¯ĞL    ¾aü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ S H C o r e . d l l    ÀÀ (4  $4  ]åCA             ¾aü   Ğ
     $4  Zò
 ¯ĞL    ¾aü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ S H C o r e . d l l    Àj (4  $4  ˆüCA           $4  \4    [«Œıÿÿ Z«Œıÿÿ  €ª    ğª          €©¹ÿû    uª          i s k  ÀÀ 
(4  $4  ÖùDA             k_ü   p     $4  7˜ üŞÌ^    k_ü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ b c r y p t . d l l    ÀÂ 
(4  $4  üEA             ™_ü   `     $4  L ?…m    ™_ü                  \ D e v i c e \ H a r d d i s k V o l u m e 2 \ W i n d o w s \ S y s t e m 3 2 \ c r y p t 3 2 . d l l   i s k  ÀD 
Ò4EA        C          À{>
    9İ%†¼ÿÿ`Å}Ú«ÿÿÆ      |2  p p  ÀD 
›8EA        C          Ğ{>
    9İ%†¼ÿÿ`Å}Ú«ÿÿª      |2  †¼ÿÿ ÀD 
b<EA        C          à{>
    9İ%†¼ÿÿ`Å}Ú«ÿÿº      |2       ÀD 
@EA        C          ğ{>
    9İ%†¼ÿÿ`Å}Ú«ÿÿ/      |2  s k  ÀD 
¼CEA        C           |>
    9İ%†¼ÿÿ`Å}Ú«ÿÿ:      |2  f t  ÀD 
öFEA        C          |>
    9İ%†¼ÿÿ`Å}Ú«ÿÿj      |2  c h  Àt%®HEA    )åAA       ì  €  |2  l  D4    Ì3  À3  D    à0  (4  ¤  Ø2  ˜3     ¥  ˆ2 ŠÒ« N€w
ŠÒÙN 	ŠÒN ŠR×NVa VÒ-" V¢  #VRA" VÒW:* VRê V¸Î^Œ‡V8% bšN€r^RSÕ [  % f—&  “