//
// A universal frame-based network protocol which can be used to exchange
// arbitrary structured data.
//
// In short:
//
// - Each transmission is comprised by one fixed-size frame.
// - Each frame contains a protocol version number.
// - Each frame contains an application frame type.
// - Each frame can contain an identifying tag.
// - Each frame can have application-specific data of up to UINT32_MAX size.
// - Transactions style messaging can be modeled on top using frame tags.
// - Lightweight API on top of libdispatch (aka GCD) -- close to the metal.
// - 
//
#include <dispatch/dispatch.h>

static const uint32_t RIOFrameVersion1 = 1;
static const uint32_t RIOFrameNoTag = 0;
static const uint32_t RIOFrameTypeEndOfStream = 0;

@interface RIOFrameProtocol : NSObject {
  dispatch_queue_t queue_;
  uint32_t nextFrameTag_;
}

// Queue on which to run data processing blocks. Can be reassigned at any point
// in time.
@property dispatch_queue_t queue;

// Initialize a new protocol object to use a specific queue.
- (id)initWithDispatchQueue:(dispatch_queue_t)queue;

// Initialize a new protocol object to use the current calling queue.
- (id)init;

#pragma mark Sending frames

// Generate a new tag that is unique within this protocol object.
- (uint32_t)newTag;

// Send a frame over *channel* with an optional payload and optional callback.
// If *callback* is not NULL, the block is invoked when either an error occured
// or when the frame (and payload, if any) has been completely sent.
- (void)sendFrameOfType:(uint32_t)frameType
                    tag:(uint32_t)tag
            withPayload:(dispatch_data_t)payload
            overChannel:(dispatch_io_t)channel
               callback:(void(^)(NSError *error))callback;

#pragma mark Receiving frames

// Read frames over *channel* as they arrive.
// The onFrame handler is responsible for reading (or discarding) any payload
// and call *resumeReadingFrames* afterwards to resume reading frames.
// To stop reading frames, simply do not invoke *resumeReadingFrames*.
// When the stream ends, a frame of type RIOFrameTypeEndOfStream is received.
- (void)readFramesOverChannel:(dispatch_io_t)channel
                      onFrame:(void(^)(NSError *error,
                                       uint32_t type,
                                       uint32_t tag,
                                       uint32_t payloadSize,
                                       dispatch_block_t resumeReadingFrames))onFrame;

// Read a single frame over *channel*. A frame of type RIOFrameTypeEndOfStream
// denotes the stream has ended.
- (void)readFrameOverChannel:(dispatch_io_t)channel
                    callback:(void(^)(NSError *error,
                                      uint32_t frameType,
                                      uint32_t frameTag,
                                      uint32_t payloadSize))callback;

#pragma mark Receiving frame payloads

// Read a complete payload. It's the callers responsibility to make sure
// payloadSize is not too large since memory will be automatically allocated
// where only payloadSize is the limit.
// The returned dispatch_data_t object owns *buffer* and thus you need to call
// dispatch_retain on *contiguousData* if you plan to keep *buffer* around after
// returning from the callback.
- (void)readPayloadOfSize:(size_t)payloadSize
              overChannel:(dispatch_io_t)channel
                 callback:(void(^)(NSError *error,
                                   dispatch_data_t contiguousData,
                                   const uint8_t *buffer,
                                   size_t bufferSize))callback;

// Discard data of *size* waiting on *channel*. *callback* can be NULL.
- (void)readAndDiscardDataOfSize:(size_t)size
                     overChannel:(dispatch_io_t)channel
                        callback:(void(^)(NSError *error, BOOL endOfStream))callback;

@end

@interface NSData (RIOFrameProtocol)
// Creates a new dispatch_data_t object which references the receiver and uses
// the receivers bytes as its backing data. The returned dispatch_data_t object
// holds a reference to the recevier. It's the callers responsibility to call
// dispatch_release on the returned object when done.
- (dispatch_data_t)createReferencingDispatchData;
@end

@interface NSDictionary (RIOFrameProtocol)
// See description of -[NSData(RIOFrameProtocol) createReferencingDispatchData]
- (dispatch_data_t)createReferencingDispatchData;

// Decode *data* as a peroperty list-encoded dictionary. Returns nil on failure.
+ (NSDictionary*)dictionaryWithContentsOfDispatchData:(dispatch_data_t)data;
@end