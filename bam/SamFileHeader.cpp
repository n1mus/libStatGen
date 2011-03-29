/*
 *  Copyright (C) 2010  Regents of the University of Michigan
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SamFileHeader.h"
#include "SamHeaderSQ.h"
#include "SamHeaderRG.h"


const std::string SamFileHeader::EMPTY_RETURN = "";

SamFileHeader::SamFileHeader()
    : myHD(NULL),
      myReferenceInfo()
{
    resetHeader();
}


SamFileHeader::~SamFileHeader()
{
    resetHeader();
}


// Copy Constructor   
SamFileHeader::SamFileHeader(const SamFileHeader& header)
{
    copy(header);
}


// Overload operator = to copy the passed in header into this header.
SamFileHeader & SamFileHeader::operator = (const SamFileHeader& header)
{
    copy(header);
    return(*this);
}


bool SamFileHeader::copy(const SamFileHeader& header)
{
    // Check to see if the passed in value is the same as this.
    if(this == &header)
    {
        return(true);
    }

    resetHeader();
    // Copy Reference contigs, hash, lengths.
    myReferenceInfo = header.myReferenceInfo;

    // Copy the records by getting the other header's header string
    // and parsing it.
    std::string newString;
    bool status = header.getHeaderString(newString);
    String newHeaderString = newString.c_str();
    
    status &= parseHeader(newHeaderString);

    myCurrentHeaderIndex = header.myCurrentHeaderIndex;
    myCurrentCommentIndex = header.myCurrentCommentIndex;

    return(status);
}


// Reset the header for a new entry, clearing out previous values.
void SamFileHeader::resetHeader()
{
    myReferenceInfo.clear();

    // Clear the pointers to the header records.  They are deleted when the
    // vector is cleaned up.
    myHD = NULL;
    mySQs.Clear();
    myRGs.Clear();
    myPGs.Clear();

    // Delete the header records and clear the vector.
    for(unsigned int headerIndex = 0; headerIndex < myHeaderRecords.size(); 
        headerIndex++)
    {
        delete myHeaderRecords[headerIndex];
        myHeaderRecords[headerIndex] = NULL;
    }
    myHeaderRecords.clear();

    // Reset the iterator for the header lines.
    resetHeaderRecordIter();

    // Reset the comment iterator.
    resetCommentIter();

    // Reset the individual type header iterators.
    resetSQRecordIter();
    resetRGRecordIter();
    resetPGRecordIter();

    // Clear the comments
    myComments.clear();
}


// Set the passed in string to the entire header string.  Clearing its
// current contents.
bool SamFileHeader::getHeaderString(std::string& header) const
{
    header.clear();
   
    // Keep getting header lines until there are no more - false returned.
    unsigned int index = 0;
    while(getHeaderLine(index, header) != false)
    {
        ++index;
    }

    return(true);
}


int SamFileHeader::getReferenceID(const String & referenceName)
{
    return(myReferenceInfo.getReferenceID(referenceName));
}


int SamFileHeader::getReferenceID(const char* referenceName)
{
    return(myReferenceInfo.getReferenceID(referenceName));
}


const String & SamFileHeader::getReferenceLabel(int id) const
{
    return(myReferenceInfo.getReferenceLabel(id));
}


// Get the Reference Information
const SamReferenceInfo* SamFileHeader::getReferenceInfo() const
{
    return(&myReferenceInfo);
}


// Add reference sequence name and reference sequence length to the header.
void SamFileHeader::addReferenceInfo(const char* referenceSequenceName, 
                                     int32_t referenceSequenceLength)
{
    myReferenceInfo.add(referenceSequenceName, referenceSequenceLength);
}

// Add a header line that has an const char* value.
bool SamFileHeader::addHeaderLine(const char* type, const char* tag, 
                                  const char* value)
{
    String headerLine;
    headerLine += "@";
    headerLine += type;
    headerLine += "\t";
    headerLine += tag;
    headerLine += ":";
    headerLine += value;
    return(addHeaderLine(headerLine.c_str()));
}


// Add a header line that is already preformatted in a const char*.
// It is assumed that the line does not contain a \n.
bool SamFileHeader::addHeaderLine(const char* headerLine)
{
    // Parse the added header line.
    String headerString = headerLine;
    if(parseHeader(headerString))
    {
        // Successfully parsed the header line.
        return(true);
    }
    // Failed to parse the header line, return false.
    return(false);
}


// Add the specified tag and value to the HD header.
bool SamFileHeader::setHDTag(const char* tag, const char* value)
{
    if(myHD == NULL)
    {
        // Need to create the HD line.
        myHD = new SamHeaderHD();
        if(myHD == NULL)
        {
            // New failed, return false.
            return(false);
        }
        // Succeeded to create the line, add it to the
        // list.
        myHeaderRecords.push_back(myHD);
    }
    return(myHD->setTag(tag, value));
}


// Add the specified tag and value to the SQ header with the specified name.
// If the header does not yet exist, the header is added.
bool SamFileHeader::setSQTag(const char* tag, const char* value,
                             const char* name)
{
    // Get the SQ record for the specified name.
    SamHeaderSQ* sq = getSQ(name);
    if(sq == NULL)
    {
        // The SQ does not yet exist.
        // Add it.
        sq = new SamHeaderSQ();

        if(sq == NULL)
        {
            // Could not create the header record.
            return(false);
        }

        // Created the header record, so add it to the list of SQ lines.
        mySQs.Add(name, sq);
        myHeaderRecords.push_back(sq);

        // Add the key tag 
        if(!sq->addKey(name))
        {
            // Failed to add the key tag, return false.
            return(false);
        }
    }

    return(sq->setTag(tag, value));
}


// Add the specified tag and value to the RG header with the read group
// identifier.  If the header does not yet exist, the header is added.
bool SamFileHeader::setRGTag(const char* tag, const char* value, const char* id)
{
    // Get the RG record for the specified name.
    SamHeaderRG* rg = getRG(id);
    if(rg == NULL)
    {
        // The RG does not yet exist.
        // Add it.
        rg = new SamHeaderRG();

        if(rg == NULL)
        {
            // Could not create the header record.
            return(false);
        }

        // Created the header record, so add it to the list of RG lines.
        myRGs.Add(id, rg);
        myHeaderRecords.push_back(rg);

        // Add the key tag 
        if(!rg->addKey(id))
        {
            // Failed to add the key tag, return false.
            return(false);
        }
    }

    return(rg->setTag(tag, value));
}


// Add the specified tag and value to the PG header with the specified id.
// If the header does not yet exist, the header is added.
// Add the specified tag and value to the PG header.
bool SamFileHeader::setPGTag(const char* tag, const char* value, const char* id)
{
    // Get the PG record for the specified name.
    SamHeaderPG* pg = getPG(id);
    if(pg == NULL)
    {
        // The PG does not yet exist.
        // Add it.
        pg = new SamHeaderPG();

        if(pg == NULL)
        {
            // Could not create the header record.
            return(false);
        }

        // Created the header record, so add it to the list of PG lines.
        myPGs.Add(id, pg);
        myHeaderRecords.push_back(pg);

        // Add the key tag 
        if(!pg->addKey(id))
        {
            // Failed to add the key tag, return false.
            return(false);
        }
    }

    return(pg->setTag(tag, value));
}


// Add the HD record to the header.
bool SamFileHeader::addHD(SamHeaderHD* hd)
{
    // If there is already an HD header or if null
    // was passed in, return false.
    if((myHD != NULL) || (hd == NULL))
    {
        return(false);
    }
    myHD = hd;
   
    myHeaderRecords.push_back(myHD);
    return(true);
}


// Add the SQ record to the header.
bool SamFileHeader::addSQ(SamHeaderSQ* sq)
{
    if(sq == NULL)
    {
        // null pointer passed in, can't add it.
        return(false);
    }
    const char* name = sq->getTagValue("SN");
    if(strcmp(name, EMPTY_RETURN.c_str()) == 0)
    {
        // SN is not set, so can't add it.
        return(false);
    }

    // Determine whether or not a record with this
    // key is already in the hash.
    if(mySQs.Find(name) < 0)
    {
        // It is not already in the hash so
        // add it.
        mySQs.Add(name, sq);
        myHeaderRecords.push_back(sq);
        return(true);
    }

    // It is already in the hash, so cannot be added.
    return(false);
}


// Add the RG record to the header.
bool SamFileHeader::addRG(SamHeaderRG* rg)
{
    if(rg == NULL)
    {
        // null pointer passed in, can't add it.
        return(false);
    }
    const char* id = rg->getTagValue("ID");
    if(strcmp(id, EMPTY_RETURN.c_str()) == 0)
    {
        // ID is not set, so can't add it.
        return(false);
    }

    // Determine whether or not a record with this
    // key is already in the hash.
    if(myRGs.Find(id) < 0)
    {
        // It is not already in the hash so
        // add it.
        myRGs.Add(id, rg);
        myHeaderRecords.push_back(rg);
        return(true);
    }

    // It is already in the hash, so cannot be added.
    return(false);
}


// Add the PG record to the header.
bool SamFileHeader::addPG(SamHeaderPG* pg)
{
    // If there is already an PG header, return false.
    if(pg == NULL)
    {
        return(false);
    }
    const char* id = pg->getTagValue("ID");
    if(strcmp(id, EMPTY_RETURN.c_str()) == 0)
    {
        // ID is not set, so can't add the header record.
        return(false);
    }

    // Determine whether or not a record with this
    // key is already in the hash.
    if(myPGs.Find(id) < 0)
    {
        // It is not already in the hash so
        // add it.
        myPGs.Add(id, pg);
        myHeaderRecords.push_back(pg);
        return(true);
    }

    // It is already in the hash, so cannot be added.
    return(false);
}


// Remove the HD record.
bool SamFileHeader::removeHD()
{
    if(myHD != NULL)
    {
        // Reset the record.  Do not delete it since it is in the headerRecords
        // vector and it is not worth the time to remove it from the middle of
        // that vector since this is the header and the space does not need
        // to be conserved.
        myHD->reset();

        // Set myHD to null so a new HD could be added.
        myHD = NULL;
    }

    return(true);
}


// Remove the SQ record associated with the specified name.
bool SamFileHeader::removeSQ(const char* name)
{
    // Look up the name in the hash.
    int hashIndex = mySQs.Find(name);
    if(hashIndex < 0)
    {
        // Not found in the hash, so nothing to
        // delete, return true it does not exist
        // in the hash.
        return(true);
    }
   
    // Get the SQ.
    SamHeaderSQ* sq = (SamHeaderSQ*)(mySQs.Object(hashIndex));

    if(sq == NULL)
    {
        // sq is null, this is an error since hashIndex was greater than 0,
        // so it should have been found.
        return(false);
    }

    // Reset the record.  Do not delete it since it is in the headerRecords
    // vector and it is not worth the time to remove it from the middle of
    // that vector since this is the header and the space does not need
    // to be conserved.
    sq->reset();

    // Delete the entry from the hash.
    mySQs.Delete(hashIndex);

    return(true);
}


// Remove the RG record associated with the specified id.
bool SamFileHeader::removeRG(const char* id)
{
    // Look up the id in the hash.
    int hashIndex = myRGs.Find(id);
    if(hashIndex < 0)
    {
        // Not found in the hash, so nothing to
        // delete, return true it does not exist
        // in the hash.
        return(true);
    }
   
    // Get the RG.
    SamHeaderRG* rg = (SamHeaderRG*)(myRGs.Object(hashIndex));

    if(rg == NULL)
    {
        // rg is null, this is an error since hashIndex was greater than 0,
        // so it should have been found.
        return(false);
    }

    // Reset the record.  Do not delete it since it is in the headerRecords
    // vector and it is not worth the time to remove it from the middle of
    // that vector since this is the header and the space does not need
    // to be conserved.
    rg->reset();

    // Delete the entry from the hash.
    myRGs.Delete(hashIndex);

    return(true);
}


// Remove the PG record associated with the specified id.
bool SamFileHeader::removePG(const char* id)
{
    // Look up the id in the hash.
    int hashIndex = myPGs.Find(id);
    if(hashIndex < 0)
    {
        // Not found in the hash, so nothing to
        // delete, return true it does not exist
        // in the hash.
        return(true);
    }
   
    // Get the PG.
    SamHeaderPG* pg = (SamHeaderPG*)(myPGs.Object(hashIndex));

    if(pg == NULL)
    {
        // pg is null, this is an error since hashIndex was greater than 0,
        // so it should have been found.
        return(false);
    }

    // Reset the record.  Do not delete it since it is in the headerRecords
    // vector and it is not worth the time to remove it from the middle of
    // that vector since this is the header and the space does not need
    // to be conserved.
    pg->reset();

    // Delete the entry from the hash.
    myPGs.Delete(hashIndex);

    return(true);
}


SamStatus::Status SamFileHeader::setHeaderFromBamFile(IFILE filePtr)
{
    if((filePtr == NULL) || (filePtr->isOpen() == false))
    {
        // File is not open, return failure.
        return(SamStatus::FAIL_ORDER);
    }

    int headerLength;
    // Read the header length.
    int readSize = ifread(filePtr, &headerLength, sizeof(int));
   
    if(readSize != sizeof(int))
    {
        // Failed to read the header length.
        return(SamStatus::FAIL_IO);
    }
   
    String header;
    if (headerLength > 0)
    {
        // Read the header.
        readSize = 
            ifread(filePtr, header.LockBuffer(headerLength + 1), headerLength);
        header[headerLength] = 0;
        header.UnlockBuffer();
        if(readSize != headerLength)
        {
            // Failed to read the header.
            return(SamStatus::FAIL_IO);
        }
    }

    // Parse the header that was read.
    parseHeader(header);
    return(SamStatus::SUCCESS);
}


const char* SamFileHeader::getHDTagValue(const char* tag)
{
    if(myHD == NULL)
    {
        // return blank since there is no HD type.
        return(EMPTY_RETURN.c_str());
    }
    return(myHD->getTagValue(tag));
}


// Get the value associated with the specified tag on the SQ line with
// the specified sequence name.
const char* SamFileHeader::getSQTagValue(const char* tag, const char* name)
{
    // Look up the name in the hash to get the associated SQ object.
    SamHeaderSQ* sq = (SamHeaderSQ*)(mySQs.Object(name));
   
    // If it is NULL - the tag was not found, so return
    if(sq == NULL)
    {
        return(EMPTY_RETURN.c_str());
    }

    // Found the object, so return the SQ Tag.
    return(sq->getTagValue(tag));
}


// Get the value associated with the specified tag on the RG line with
// the specified read group identifier.
const char* SamFileHeader::getRGTagValue(const char* tag, const char* id)
{
    // Look up the id in the hash to get the associated RG object.
    SamHeaderRG* rg = (SamHeaderRG*)(myRGs.Object(id));
   
    // If it is NULL - the tag was not found, so return
    if(rg == NULL)
    {
        return(EMPTY_RETURN.c_str());
    }

    // Found the object, so return the RG Tag.
    return(rg->getTagValue(tag));
}


const char* SamFileHeader::getPGTagValue(const char* tag, const char* id)
{
    // Look up the id in the hash to get the associated PG object.
    SamHeaderPG* pg = (SamHeaderPG*)(myPGs.Object(id));
   
    // If it is NULL - the tag was not found, so return
    if(pg == NULL)
    {
        return(EMPTY_RETURN.c_str());
    }

    // Found the object, so return the PG Tag.
    return(pg->getTagValue(tag));
}


// Get the number of SQ objects.
int SamFileHeader::getNumSQs()
{
    return(mySQs.Entries());
}


// Get the number of RG objects.
int SamFileHeader::getNumRGs()
{
    return(myRGs.Entries());
}


// Get the number of PG objects.
int SamFileHeader::getNumPGs()
{
    return(myPGs.Entries());
}


// Get the HD object.
SamHeaderHD* SamFileHeader::getHD()
{
    return(myHD);
}


// Get the SQ object with the specified sequence name.
SamHeaderSQ* SamFileHeader::getSQ(const char* name)
{
    return((SamHeaderSQ*)(mySQs.Object(name)));
}


// Get the RG object with the specified read group identifier.
SamHeaderRG* SamFileHeader::getRG(const char* id)
{
    return((SamHeaderRG*)(myRGs.Object(id)));
}


// Get the PG object.
SamHeaderPG* SamFileHeader::getPG(const char* id)
{
    return((SamHeaderPG*)(myPGs.Object(id)));
}


// Return the value of the SO tag.  
// If this field does not exist, EMPTY_RETURN.c_str() is returned.
const char* SamFileHeader::getSortOrder()
{
    if(myHD == NULL)
    {
        // No HD, so return blank EMPTY_RETURN.c_str()
        return(EMPTY_RETURN.c_str());
    }
    return(myHD->getSortOrder());   
}


// Deprecated way of getting the sort order from the file.
const char* SamFileHeader::getTagSO()
{
    return(getSortOrder());
}


// Get the next SQ header record.  After all SQ headers have been retrieved,
// NULL is returned until a reset is called.
SamHeaderRecord* SamFileHeader::getNextSQRecord()
{
    return(getNextHeaderRecord(myCurrentSQIndex, 
                               SamHeaderRecord::SQ));
}


// Get the next RG header record.  After all RG headers have been retrieved,
// NULL is returned until a reset is called.
SamHeaderRecord* SamFileHeader::getNextRGRecord()
{
    return(getNextHeaderRecord(myCurrentRGIndex, 
                               SamHeaderRecord::RG));
}


// Get the next PG header record.  After all PG headers have been retrieved,
// NULL is returned until a reset is called.
SamHeaderRecord* SamFileHeader::getNextPGRecord()
{
    return(getNextHeaderRecord(myCurrentPGIndex, 
                               SamHeaderRecord::PG));
}


// Reset to the beginning of the header records so the next call
// to getNextSQRecord returns the first SQ header record.
void SamFileHeader::resetSQRecordIter()
{
    myCurrentSQIndex = 0;
}


// Reset to the beginning of the header records so the next call
// to getNextRGRecord returns the first RG header record.
void SamFileHeader::resetRGRecordIter()
{
    myCurrentRGIndex = 0;
}


// Reset to the beginning of the header records so the next call
// to getNextPGRecord returns the first PG header record.
void SamFileHeader::resetPGRecordIter()
{
    myCurrentPGIndex = 0;
}


// Get the next header record of the specified type.
// Pass in the index to start looking at and the type to look for.
// Update the index.
// After all headers of that type have been retrieved,
// NULL is returned until a reset is called for that type.
SamHeaderRecord* SamFileHeader::getNextHeaderRecord(uint32_t& index, 
                                                    SamHeaderRecord::SamHeaderRecordType headerType)
{
    SamHeaderRecord* foundRecord = NULL;
    // Loop until a record is found or until out of range of the 
    // headerRecord vector.
    while((index < myHeaderRecords.size()) 
          && (foundRecord == NULL))
    {
        // Get the next record.
        foundRecord = myHeaderRecords[index];
        // Either way, increment the index.
        ++index;
        // Check to see if the next record is active.
        if(!foundRecord->isActiveHeaderRecord())
        {
            // Not active, so clear the pointer.
            foundRecord = NULL;
        }
        // Check to see if the record is the right type.
        else if(foundRecord->getType() != headerType)
        {
            // Not the right type, so clear the pointer.
            foundRecord = NULL;
        }
    }

    // Return the record if it was found.  Will be null if none were found.
    return(foundRecord);
}


// Get the next header record.  After all headers have been retrieved,
// NULL is returned until a reset is called.  Does not return the
// Comment lines.
// NOTE: both getNextHeaderRecord and getNextHeaderLine increment the
// same iterator.
SamHeaderRecord* SamFileHeader::getNextHeaderRecord()
{
    // Get the next header record
    SamHeaderRecord* foundRecord = NULL;
    // Loop until a record is found or until out of range of the 
    // headerRecord vector.
    while((myCurrentHeaderIndex < myHeaderRecords.size()) 
          && (foundRecord == NULL))
    {
        // Get the next record.
        foundRecord = myHeaderRecords[myCurrentHeaderIndex];
        // Either way, increment the index.
        ++myCurrentHeaderIndex;
        // Check to see if the next record is active.
        if(!foundRecord->isActiveHeaderRecord())
        {
            // Not active, so clear the pointer.
            foundRecord = NULL;
        }
    }

    // Return the record if it was found.  Will be null if none were found.
    return(foundRecord);
}


// Set the passed in string to the next header line.  The passed in 
// string will be overwritten.  If there are no more header lines or there
// is an error, false is returned and the passed in string is set to EMPTY_RETURN.c_str()
// until a rest is called.
// Will also return the comment lines.
// NOTE: both getNextHeaderRecord and getNextHeaderLine increment the
// same iterator.
bool SamFileHeader::getNextHeaderLine(std::string &headerLine)
{
    headerLine = EMPTY_RETURN.c_str();

    // Until the header is set, keep reading.
    // Header could return EMPTY_RETURN.c_str() if the header line is blank.
    while(headerLine == EMPTY_RETURN.c_str())
    {
        if(getHeaderLine(myCurrentHeaderIndex, headerLine) == false)
        {
            // getHeaderLine failed, so stop processing, and return false.
            return(false);
        }
        else
        {
            // In range, increment the index.
            ++myCurrentHeaderIndex;
        }
    }
    return(true);
}


// Reset to the beginning of the header records so the next call
// to getNextHeaderRecord returns the first header line.
void SamFileHeader::resetHeaderRecordIter()
{
    myCurrentHeaderIndex = 0;
}


// Returns the comment on the next comment line.  Returns EMPTY_RETURN.c_str() if all comment
// lines have been returned, until resetCommentIter is called.
const char* SamFileHeader::getNextComment()
{
    if(myCurrentCommentIndex < myComments.size())
    {
        return(myComments[myCurrentCommentIndex++].c_str());
    }
    // Already gone through all the comments, return EMPTY_RETURN.c_str().
    return(EMPTY_RETURN.c_str());
}


// Resets to the beginning of the comments so getNextComment returns
// the first comment.
void SamFileHeader::resetCommentIter()
{
    myCurrentCommentIndex = 0;
}


// Add a comment.
bool SamFileHeader::addComment(const char* comment)
{
    if((comment != NULL) && (strcmp(comment, EMPTY_RETURN.c_str()) != 0))
    {
        // Valid comment, so add it.
        myComments.push_back(comment);
    }
    return(true);
}


// Populate the reference info from the SQ fields.
void SamFileHeader::generateReferenceInfo()
{
    // Loop through the SQ fields.
    uint32_t sqIndex = 0;
    SamHeaderRecord* hdrRec = getNextHeaderRecord(sqIndex, SamHeaderRecord::SQ);
    while(hdrRec != NULL)
    {
        // Set the reference info based on this SQ record.
        String refName = hdrRec->getTagValue("SN");
        String refLen = hdrRec->getTagValue("LN");
        long refLenInt = 0;
        if(refLen.AsInteger(refLenInt))
        {
            // Successfully converted the reference to an integer
            // so add the reference information.
            myReferenceInfo.add(refName, refLen);
        }
        hdrRec = getNextHeaderRecord(sqIndex, SamHeaderRecord::SQ);
    }
}


// Parse the header.
bool SamFileHeader::parseHeader(String& header)
{
    // Track the parsing status.
    // If there are any badly formatted fields found in parsing
    // it will be set to false.
    bool status = true;

    // Split the header into lines.
    std::vector<String>* types = header.Split('\n');

    // Loop through each header line, parsing that line.
    for(uint32_t index = 0; index < types->size(); index++)
    {
        // Parse the header line.
        status &= parseHeaderLine(types->at(index));
    }
   
    // Delete the types vector.
    delete types;
    types = NULL;

    return(status);
}


// Parse one line of the header.
bool SamFileHeader::parseHeaderLine(const String& headerLine)
{
    StringArray tokens;

    // Split the line by tabs.
    tokens.ReplaceColumns(headerLine, '\t');
   
    if(tokens.Length() < 1)
    {
        // Nothing on this line, just return true.
        return(true);
    }
   
    // Get the header type, the first column.
    if((tokens[0].Length() != 3) || (tokens[0][0] != '@'))
    {
        // The header type string is incorrect.  Should be 3 characters
        // with the first one @.
        return(false);
    }
   
    bool status = true;
    if(tokens[0] == "@HD")
    {
        if(myHD == NULL)
        {
            // Create a new hd.
            myHD = new SamHeaderHD();
            if(myHD == NULL)
            {
                // Failed to allocate HD, so return false.
                return(false);
            }
            myHeaderRecords.push_back(myHD);
            status &= myHD->setFields(tokens);
        }
        else
        {
            // HD already set, so return false.
            status = false;
        }
    }
    else if(tokens[0] == "@SQ")
    {
        // Create a new SQ record.
        SamHeaderSQ* sq = new SamHeaderSQ();
      
        if(sq->setFields(tokens))
        {
            // sq fields were properly set, so add it to the list of
            // SQ lines.
            status &= addSQ(sq);
        }
        else
        {
            status = false;
        }
    }
    else if(tokens[0] == "@RG")
    {
        // Create a new RG record.
        SamHeaderRG* rg = new SamHeaderRG();
      
        if(rg->setFields(tokens))
        {
            // rg fields were properly set, so add it to the list of
            // RG lines.
            status &= addRG(rg);
        }
        else
        {
            status = false;
        }
    }
    else if(tokens[0] == "@PG")
    {
        // Create a new PG record.
        SamHeaderPG* pg = new SamHeaderPG();
      
        if(pg->setFields(tokens))
        {
            // pg fields were properly set, so add it to the list of
            // PG lines.
            status &= addPG(pg);
        }
        else
        {
            status = false;
        }
    }
    else if(tokens[0] == "@CO")
    {
        addComment(tokens[1]);
    }
    else
    {
        // Unknown header type.
        status = false;
    }
   
    return(status);
}



// Set the passed in string to the header line at the specified index.
// It does NOT clear the current contents of header.
// NOTE: some indexes will return blank if the entry was deleted.
bool SamFileHeader::getHeaderLine(unsigned int index, std::string& header) const
{
    // Check to see if the index is in range of the header records vector.
    if(index < myHeaderRecords.size())
    {
        // In range of the header records vector, so get the string for
        // that record.
        SamHeaderRecord* hdrRec = myHeaderRecords[index];
        hdrRec->appendString(header);
        return(true);
    }
    else
    {
        unsigned int commentIndex = index - myHeaderRecords.size();
        // Check to see if it is in range of the comments.
        if(commentIndex < myComments.size())
        {
            // It is in range of the comments, so add the type.
            header += "@CO\t";
            // Add the comment.
            header += myComments[commentIndex];
            // Add the new line.
            header += "\n";
            return(true);
        }
    }
    // Invalid index.
    return(false);
}
