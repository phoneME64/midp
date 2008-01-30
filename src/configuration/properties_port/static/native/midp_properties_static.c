/*
 * Copyright  1990-2008 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 *
 */

#include <string.h>
#include <ctype.h>

#include <kni.h>

#include <midpError.h>
#include <midpMalloc.h>
#include <midp_properties_port.h>

/* generated by Configurator */
#include <midp_property_callouts.h>

extern char* midp_internalKey[];
extern char* midp_internalValue[];
extern char* midp_systemKey[];
extern char* midp_systemValue[];

/**
 * @file
 * MIDP native configuration property implementation.
 * <b>OVERVIEW:<b>
 * <p>
 * This file defines the functions that access runtime configuration
 * properties that are predefined in configuration files at
 * compilation time or overridden by system calls.
 * <p>
 * <b>DATASTRUCTURE:<b>
 * <p>
 * The properties are separated into two lists. One for properties
 * used by the internal implementation and one for properties visible
 * to a MIDlet. Each property list is a linked list of key/value
 * pairs.
 */


/** Storage structure for a property set */
typedef struct _configproperty {
    struct _configproperty *next ;
    const char *key;
    const char *value;
    unsigned char flags;
} Property ;

/** Flag to signify the property's key has been allocated */
#define NEW_KEY_FLAG   0x01
/** Flag to signify the property's value has been allocated */
#define NEW_VALUE_FLAG 0x02

/** Sets the new key flag */
#define SET_NEW_KEY(p)     ((p)->flags |= NEW_KEY_FLAG)
/** Sets the new value flag */
#define SET_NEW_VALUE(p)   ((p)->flags |= NEW_VALUE_FLAG)
/** Unsets the new key flag */
#define UNSET_NEW_KEY(p)   ((p)->flags &= ~NEW_KEY_FLAG)
/** Unsets the new value flag */
#define UNSET_NEW_VALUE(p) ((p)->flags &= ~NEW_VALUE_FLAG)
/** Tests if the new key flag has been set */
#define IS_NEW_KEY(p)      ((p)->flags & NEW_KEY_FLAG)
/** Tests if the new value flag has been set */
#define IS_NEW_VALUE(p)    ((p)->flags & NEW_VALUE_FLAG)
/** Clears all flags */
#define CLEAR_FLAGS(p)     ((p)->flags = 0)

/** Property set for internal properties */
static Property* internalPropertySet = NULL;
/** Property set for system properties */
static Property* systemPropertySet = NULL;


/**
 * Calls out to the system to obtain a property.
 *
 * @param key The key to search for
 *
 * @return The value returned by the system if the key has a
 *         callout method defined, otherwise <tt>NULL<tt>
 */
static char* doCallout(const char* key) {
    int i;
    for (i = 0; calloutKey[i] != NULL; ++i) {
        if (strcmp(key, calloutKey[i]) == 0) {
            return (calloutFunction[i])();
        }
    }

    return NULL;
}

/**
 * Sets a property key to the specified value. If the key does
 * not exist and <tt>useMalloc<tt> is <tt>KNI_TRUE<tt>, use
 * <tt>midpMalloc()<tt> to make a copy of the contents of
 * <tt>key<tt>. If the key does not exist and <tt>useMalloc<tt>
 * is <tt>KNI_FALSE<tt>, make a copy of the pointer to
 * <tt>key<tt>. If the <tt>key<tt> already exists in the
 * property lists, modify its associated value.
 *
 * @param propertySet The property set in which to add/modify
 *                    <tt>key<tt>
 * @param key The key to set
 * @param value The value to associate with <tt>key<tt>
 * @param useMalloc If <tt>KNI_TRUE<tt>, use <tt>midpMalloc()<tt>
 *                  to make a copy of the contents of <tt>value<tt>,
 *                  otherwise make a copy of the pointer to
 *                  <tt>value<tt>.
 */
static void
setProp(Property** propertySet, const char* key, const char* value,
        jboolean useMalloc) {
    Property *p;
    for (p = *propertySet; p; p = p->next) {
        if (strcmp(key, p->key) == 0) {
            if (IS_NEW_VALUE(p)) {
                midpFree((void *)p->value);
            }
            if (useMalloc) {
                SET_NEW_VALUE(p);
                p->value = midpStrdup(value);
            } else {
                UNSET_NEW_VALUE(p);
                p->value = value;
            }
            /*
             * if midpStrdup fails we will just return without setting
             * the value to anything other than NULL
             */
            return;
        }
    }

    /* If the value is not defined, add it now */
    p = (Property*)midpMalloc(sizeof(Property));
    if (NULL != p){
        p->next = *propertySet;
        CLEAR_FLAGS(p);
        if (useMalloc) {
            SET_NEW_VALUE(p);
            SET_NEW_KEY(p);
            p->key = midpStrdup(key);
            p->value = midpStrdup(value);
        } else {
            UNSET_NEW_VALUE(p);
            UNSET_NEW_KEY(p);
            p->value = value;
            p->key = key;
        }

        if ((p->key == NULL) || (p->value == NULL && value != NULL)) {
            /* do nothing if there is no memory */
            if (useMalloc) {
                midpFree((void *)p->key);
                midpFree((void *)p->value);
            }
            midpFree((void *)p);
            return;
        }
        *propertySet = p ;
    }
}

/**
 * Finds a property key and returns its value.
 *
 * @param propertySet The property set to search
 * @param key The key to search for
 *
 * @return The value associated with <tt>key<tt> if found, otherwise
 *         <tt>NULL<tt>
 */
static const char*
getProp(Property** propertySet, const char* key) {
    Property* p;

    p = *propertySet;
    while (p) {
        if (strcmp(key, p->key) == 0) {
            if (NULL == p->value) {
                return doCallout(p->key); 
            }
            return (p->value);
        }
        p = p->next;
    }
    return NULL;
}

/**
 * Initializes the given property set with the specified keys
 * and values.
 * <p>
 * <b>NOTE:<b> The given <tt>key<tt> and <tt>value<tt> arrays
 * must have the same number of elements.
 *
 * @param propertySet The property set to initialize
 * @param key An array of keys to set
 * @param value An array of values to associate with the keys
 */
static void
initializeProp(Property** propertySet, const char* key[], char* value[]) {
    int i = 0;

    while (key[i] != NULL) {
        setProp(propertySet, key[i], value[i], KNI_FALSE);
        i++;
    }
}

/**
 * Frees any resources.
 *
 * @param propertySet The property set to finalize
 */
static void
finalizeProp(Property** propertySet) {
    Property* p;

    p = *propertySet;
    while (p) {
        Property* tmp;

        if (IS_NEW_KEY(p)) {
            midpFree((void *)p->key);
        }
        if (IS_NEW_VALUE(p)) {
            midpFree((void *)p->value);
        }
        tmp = p->next;
        midpFree((void *)p);
        p = tmp;
    }
    *propertySet = NULL;
}

/**
 * Initializes the configuration sub-system.
 *
 * @return <tt>0<tt> for success, otherwise a non-zero value
 */
int
initializeConfig(void) {
    initializeProp(&internalPropertySet, (const char*)midp_internalKey,
        midp_internalValue);
    initializeProp(&systemPropertySet, (const char*)midp_systemKey,
        midp_systemValue);

    return 0;
}

/**
 * Finalize the configuration subsystem by releasing all the
 * allocating memory buffers. This method should only be called by
 * midpFinalize or internally by initializeConfig.
 */
void
finalizeConfig(void) {
    finalizeProp(&internalPropertySet);
    finalizeProp(&systemPropertySet);
}

/**
 * Sets a property key to the specified value in the internal
 * property set.
 *
 * @param key The key to set
 * @param value The value to set <tt>key<tt> to
 */
void
setInternalProperty(const char* key , const char* value) {
    setProp(&internalPropertySet, key, value, KNI_TRUE);
}

/**
 * Gets the value of the specified property key in the internal
 * property set. If the key is not found in the internal property
 * set, the application property set is then searched.
 *
 * @param key The key to search for
 *
 * @return The value associated with <tt>key<tt> if found, otherwise
 *         <tt>NULL<tt>
 */
const char*
getInternalProperty(const char* key) {
    const char* value;

    value = getProp(&internalPropertySet, key);
    if (NULL == value) {
        return getProp(&systemPropertySet, key);
    }
    return value;
}

/**
 * Gets the integer value of the specified property key in the internal
 * property set. If the key is not found in the internal property
 * set, the application and afterwards the dynamic property sets
 * are searched.  
 *
 * @param key The key to search for
 *
 * @return The value associated with <tt>key</tt> if found, otherwise
 *         <tt>0</tt>
 */
int getInternalPropertyInt(const char* key) {
    const char *tmp;    

    tmp = getInternalProperty(key); 

    return(NULL == tmp) ? 0 : atoi(tmp);
}


/**
 * Gets the value of the specified property key in the internal
 * property set. If the key is not found in the internal property
 * set, the application property set is then searched. If neither
 * search finds the specified key, a default value is returned.
 *
 * @param key The key to search for
 * @param def The default value to return if <tt>key<tt> is not found.
 *
 * @return The value associated with <tt>key<tt> if found, otherwise
 *         <tt>def<tt>
 */
const char*
getInternalPropDefault(const char* key, const char* defValue) {
    const char* value;

    value = getInternalProperty(key);
    if (NULL == value) {
        return defValue;
    } else {
        return value;
    }
}

/**
 * Sets a property key to the specified value in the application
 * property set.
 *
 * @param key The key to set
 * @param value The value to set <tt>key<tt> to
 */
void
setSystemProperty(const char* key , const char* value) {
    setProp(&systemPropertySet, key, value, KNI_TRUE);
}

/**
 * Gets the value of the specified property key in the application
 * property set.
 *
 * @param key The key to search for
 *
 * @return The value associated with <tt>key<tt> if found, otherwise
 *         <tt>NULL<tt>
 */
const char*
getSystemProperty(const char* key) {
    return getProp(&systemPropertySet, key);
}
