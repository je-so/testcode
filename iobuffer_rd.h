/* title: IOBuffer-for-Reading

   Erlaubt das Lesen einer Datei vom Anfang zum Ende hin.
   Nur ein kleiner Ausschnitt des Dateiinhaltes wird dabei
   im Speicher (Buffer-Cache) gehalten.


   about: Copyright
   This program is free software.
   You can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   Author:
   (C) 2014 Jörg Seebohn

   file: C-kern/api/io/buffer/iobuffer_rd.h
    Header file <IOBuffer-for-Reading>.

   file: C-kern/io/buffer/iobuffer_rd.c
    Implementation file <IOBuffer-for-Reading impl>.
*/
#ifndef CKERN_IO_BUFFER_IOBUFFER_RD_HEADER
#define CKERN_IO_BUFFER_IOBUFFER_RD_HEADER

#include "C-kern/api/memory/memblocklist.h"

/* typedef: struct iobuffer_rd_t
 * Export <iobuffer_rd_t> into global namespace. */
typedef struct iobuffer_rd_t iobuffer_rd_t;


// section: Functions

// group: test

#ifdef KONFIG_UNITTEST
/* function: unittest_io_buffer_iobuffer_rd
 * Test <iobuffer_rd_t> functionality. */
int unittest_io_buffer_iobuffer_rd(void);
#endif


/* struct: iobuffer_rd_t
 * Dieser IOBuffer unterstützt den lesenden Zugriff.
 *
 * TODO: redraw with utf8 chars !!
 *
 * >  geblockter Dateiinhalt im Speicher (cached IOBlocks)
 * >
 * > ------<----------------------<----------------------<---------------------<---------------------<-
 * > |  --------------------    --------------------    --------------------    --------------------  |
 * > -> |                  | -> |                  | -> |                  | -> |                  | -|
 * >    --------------------    --------------------    --------------------    --------------------
 * >                            ^         ^             ^    ^
 * >                            |         (data)        |    (data)
 * >                            |         windowstart   |    windowend
 * >                            fileoffset              (fileoffset + blocksize)
 *
 * TODO: describe type
 *
 * TODO: extend vmblock_t // (no more split, verschiedene dateioffsets – pro page einen !)
 * TODO: erweitere commit Dokumentation !!
 *
 * TODO: Beschreibe per Zeichnung das Sliding Window
 *
 * Dateilänge – undefiniertes Verhalten:
 * Wenn sich die Dateilänge ändert, nachdem der Reader
 * initialisiert wurde, dann ist das Verhalten undefiniert.
 * Selbst bei Anpassung von <filesize> kann dies nicht vermieden werden.
 *
 * Grund:
 * Der Reader verwendet - wenn möglich - »memory mapped« IO.
 * Wird die Länge der Datei daher kleiner, nachdem sie in den Speicher
 * gemappt wurde, kann es beim Lesen des Speichers zu einem
 * »Segmentation Fault« kommen (hängt von dem darunter liegenden OS ab).
 * Da Längenänderung der Datei genau zum selben Zeitpunkt von einem anderen
 * Prozess durchgeführt werden kann, ist dieses Problem mit mmap-IO nicht zu umgehen.
 *
 * */
struct iobuffer_rd_t {
   // group: private fields
   uint16_t    windowstart;
   uint16_t    windowend;
   uint16_t    blocksize;
   uint16_t    nrblock;
   /* variable: buffer
    * Ein einziger großer Speicherblock, der <nrblock> Blöcke der Größe <blocksize> enthält. */
   memblock_t  buffer;
   /* variable: fileoffset
    * Offset in Bytes, ab dem der Dateiinhalt im Speicher gehalten wird.
    * Der Offset bezieht sich auf aktuellen Block, der von <windowstart>
    * referenziert wird. */
   off_t       fileoffset;
   /* variable: filesize
    * Die Dateilänge. Wird bei Initialisierung einmal gesetzt
    * und nicht mehr angepasst. */
   off_t       filesize;
};

// group: lifetime

/* define: iobuffer_rd_FREE
 * Static initializer. */
#define iobuffer_rd_FREE \
         { 0, 0 }

/* function: init_iobufferrd
 * TODO: Describe Initializes object. */
int init_iobufferrd(/*out*/iobuffer_rd_t * iobuf, uint16_t maxwindowsize);

/* function: free_iobufferrd
 * TODO: Describe Frees all associated resources. */
int free_iobufferrd(iobuffer_rd_t * iobuf);

// group: query

/* function: windowdata_iobufferrd
 * TODO: describe */
const struct memblocklist_t * windowdata_iobufferrd(const iobuffer_rd_t * iobuf);

// group: update

/* function: markunread_iobufferrd
 * TODO: Describe */
int markunread_iobufferrd(iobuffer_rd_t * iobuf, uint32_t readbytes);

/* function: clearwindow_iobufferrd
 * Entspricht <shrinkwindow_iobufferrd>(iobuf, 0). */
int clearwindow_iobufferrd(iobuffer_rd_t * iobuf);

/* function: shrinkwindow_iobufferrd
 * Inkrementiere windowstart bis (windowsize==windowend-windowstart).
 * Falls (windowsize>windowend-windowstart) wird EINVAL zurückgegeben. */
int shrinkwindow_iobufferrd(iobuffer_rd_t * iobuf, uint32_t windowsize);

/* function: readnext_iobufferrd
 * Gibt Datenadresse von windowend bis IOBlock-Ende in Parameter data zurück.
 * Falls der aktulle IOBlock komplett gelesen wurde, wird ein neuer gelesen
 * und dessen Speicheradresse in data zurückgegeben.
 * Nach Erfolg wird windowend inkrementiert bzw. zeigt auf das Ende des neu
 * eingelesenen IOBlocks.
 *
 * Ist (windowend-windowstart>maxwindowsize) wird EOVERFLOW zurückgegeben.
 * In diesem Fall ist es erforderlich das Fenster zu verkleineren (<shrinkwindow_iobufferrd>),
 * bevor weiter gelesen werden kann. Der Wert maxwindowsize wurde in <init_iobufferrd> gesetzt. */
int readnext_iobufferrd(iobuffer_rd_t * iobuf, /*out*/memblock_t * data);


// section: inline implementation

/* define: init_iobufferrd
 * Implements <iobuffer_rd_t.init_iobufferrd>. */
#define init_iobufferrd(obj) \
         // TODO: implement


#endif
