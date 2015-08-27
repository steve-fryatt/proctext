/* Copyright 2009-2015, Stephen Fryatt
 *
 * This file is part of ProcText:
 *
 *   http://www.stevefryatt.org.uk/software/
 *
 * Licensed under the EUPL, Version 1.1 only (the "Licence");
 * You may not use this work except in compliance with the
 * Licence.
 *
 * You may obtain a copy of the Licence at:
 *
 *   http://joinup.ec.europa.eu/software/page/eupl
 *
 * Unless required by applicable law or agreed to in
 * writing, software distributed under the Licence is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the Licence for the specific language governing
 * permissions and limitations under the Licence.
 */

/**
 * \file: dataxfer.h
 *
 * Save dialogue and data transfer implementation.
 */

#ifndef PROCTEXT_OLDDATAXFER
#define PROCTEXT_OLDDATAXFER

/* ==================================================================================================================
 * Static constants
 */


#define TEXT_FILE_TYPE 0xfff

#define DRAG_SAVE_PDF    1
#define DRAG_SAVE_SAVEAS 2

/**
 * Initialise the data transfer system.
 */

void olddataxfer_initialise(void);

#endif

