//
//  EntityItem.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityItem_h
#define hifi_EntityItem_h

#include <stdint.h>

#include <glm/glm.hpp>

#include <AnimationCache.h> // for Animation, AnimationCache, and AnimationPointer classes
#include <CollisionInfo.h>
#include <Octree.h> // for EncodeBitstreamParams class
#include <OctreeElement.h> // for OctreeElement::AppendState
#include <OctreePacketData.h>
#include <ShapeInfo.h>

#include "EntityItemID.h" 
#include "EntityItemProperties.h" 
#include "EntityItemPropertiesDefaults.h" 
#include "EntityTypes.h"

class EntityTree;
class EntityTreeElement;
class EntityTreeElementExtraEncodeData;

#define DONT_ALLOW_INSTANTIATION virtual void pureVirtualFunctionPlaceHolder() = 0;
#define ALLOW_INSTANTIATION virtual void pureVirtualFunctionPlaceHolder() { };

#define debugTime(T, N) qPrintable(QString("%1 [ %2 ago]").arg(T, 16, 10).arg(formatUsecTime(N - T), 15))
#define debugTimeOnly(T) qPrintable(QString("%1").arg(T, 16, 10))
#define debugTreeVector(V) V << "[" << V << " in meters ]"


/// EntityItem class this is the base class for all entity types. It handles the basic properties and functionality available
/// to all other entity types. In particular: postion, size, rotation, age, lifetime, velocity, gravity. You can not instantiate
/// one directly, instead you must only construct one of it's derived classes with additional features.
class EntityItem  {
    friend class EntityTreeElement;
public:
    enum EntityDirtyFlags {
        DIRTY_POSITION = 0x0001,
        DIRTY_VELOCITY = 0x0002,
        DIRTY_MASS = 0x0004,
        DIRTY_COLLISION_GROUP = 0x0008,
        DIRTY_MOTION_TYPE = 0x0010,
        DIRTY_SHAPE = 0x0020,
        DIRTY_LIFETIME = 0x0040,
        DIRTY_UPDATEABLE = 0x0080,
    };

    DONT_ALLOW_INSTANTIATION // This class can not be instantiated directly
    
    EntityItem(const EntityItemID& entityItemID);
    EntityItem(const EntityItemID& entityItemID, const EntityItemProperties& properties);
    virtual ~EntityItem();

    // ID and EntityItemID related methods
    QUuid getID() const { return _id; }
    void setID(const QUuid& id) { _id = id; }
    uint32_t getCreatorTokenID() const { return _creatorTokenID; }
    void setCreatorTokenID(uint32_t creatorTokenID) { _creatorTokenID = creatorTokenID; }
    bool isNewlyCreated() const { return _newlyCreated; }
    bool isKnownID() const { return getID() != UNKNOWN_ENTITY_ID; }
    EntityItemID getEntityItemID() const { return EntityItemID(getID(), getCreatorTokenID(), getID() != UNKNOWN_ENTITY_ID); }

    // methods for getting/setting all properties of an entity
    virtual EntityItemProperties getProperties() const;
    
    /// returns true if something changed
    virtual bool setProperties(const EntityItemProperties& properties);

    /// Override this in your derived class if you'd like to be informed when something about the state of the entity
    /// has changed. This will be called with properties change or when new data is loaded from a stream
    virtual void somethingChangedNotification() { }

    void recordCreationTime();    // set _created to 'now'
    quint64 getLastSimulated() const { return _lastSimulated; } /// Last simulated time of this entity universal usecs
    void setLastSimulated(quint64 now) { _lastSimulated = now; }

     /// Last edited time of this entity universal usecs
    quint64 getLastEdited() const { return _lastEdited; }
    void setLastEdited(quint64 lastEdited) 
        { _lastEdited = _lastUpdated = lastEdited; _changedOnServer = glm::max(lastEdited, _changedOnServer); }
    float getEditedAgo() const /// Elapsed seconds since this entity was last edited
        { return (float)(usecTimestampNow() - getLastEdited()) / (float)USECS_PER_SECOND; }

    void markAsChangedOnServer() {  _changedOnServer = usecTimestampNow();  }
    quint64 getLastChangedOnServer() const { return _changedOnServer; }

    // TODO: eventually only include properties changed since the params.lastViewFrustumSent time
    virtual EntityPropertyFlags getEntityProperties(EncodeBitstreamParams& params) const;
        
    virtual OctreeElement::AppendState appendEntityData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                                EntityTreeElementExtraEncodeData* entityTreeElementExtraEncodeData) const;

    virtual void appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params, 
                                    EntityTreeElementExtraEncodeData* entityTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount, 
                                    OctreeElement::AppendState& appendState) const { /* do nothing*/ };

    static EntityItemID readEntityItemIDFromBuffer(const unsigned char* data, int bytesLeftToRead, 
                                    ReadBitstreamToTreeParams& args);

    virtual int readEntityDataFromBuffer(const unsigned char* data, int bytesLeftToRead, ReadBitstreamToTreeParams& args);

    virtual int readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead, 
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData) 
                                                { return 0; }

    virtual void render(RenderArgs* args) { } // by default entity items don't know how to render

    static int expectedBytes();

    static void adjustEditPacketForClockSkew(unsigned char* codeColorBuffer, size_t length, int clockSkew);

    // perform update
    virtual void update(const quint64& now) { _lastUpdated = now; }
    quint64 getLastUpdated() const { return _lastUpdated; }
    
    // perform linear extrapolation for SimpleEntitySimulation
    void simulate(const quint64& now);
    void simulateKinematicMotion(float timeElapsed);

    virtual bool needsToCallUpdate() const { return false; }

    virtual void debugDump() const;
    
    virtual bool supportsDetailedRayIntersection() const { return false; }
    virtual bool findDetailedRayIntersectionInMeters(const glm::vec3& origin, const glm::vec3& direction,
                         bool& keepSearching, OctreeElement*& element, float& distance, BoxFace& face, 
                         void** intersectedObject, bool precisionPicking) const { return true; }

    // attributes applicable to all entity types
    EntityTypes::EntityType getType() const { return _type; }
    glm::vec3 getPositionInDomainUnits() const { return _position / (float)TREE_SCALE; } /// get position in domain scale units (0.0 - 1.0)
    const glm::vec3& getPositionInMeters() const { return _position; } /// get position in meters
    
    /// set position in domain scale units (0.0 - 1.0)
    void setPositionInDomainUnits(const glm::vec3& value)
            { setPositionInMeters(glm::clamp(value, 0.0f, 1.0f) * (float)TREE_SCALE); }
    void setPositionInMeters(const glm::vec3& value) { _position = value; }

    glm::vec3 getCenterInDomainUnits() const { return getCenterInMeters() / (float) TREE_SCALE; }
    glm::vec3 getCenterInMeters() const;

    glm::vec3 getDimensionsInDomainUnits() const { return _dimensions / (float)TREE_SCALE; } /// get dimensions in domain scale units (0.0 - 1.0)
    const glm::vec3& getDimensionsInMeters() const { return _dimensions; } /// get dimensions in meters
    float getLargestDimensionInDomainUnits() const { return glm::length(_dimensions); } /// get the largest possible dimension

    /// set dimensions in domain scale units (0.0 - 1.0)
    virtual void setDimensionsInDomainUnits(const glm::vec3& value) { _dimensions = glm::abs(value) * (float)TREE_SCALE; }

    /// set dimensions in meter units (0.0 - TREE_SCALE)
    virtual void setDimensionsInMeters(const glm::vec3& value) { _dimensions = glm::abs(value); }

    const glm::quat& getRotation() const { return _rotation; }
    void setRotation(const glm::quat& rotation) { _rotation = rotation; }

    float getGlowLevel() const { return _glowLevel; }
    void setGlowLevel(float glowLevel) { _glowLevel = glowLevel; }

    float getLocalRenderAlpha() const { return _localRenderAlpha; }
    void setLocalRenderAlpha(float localRenderAlpha) { _localRenderAlpha = localRenderAlpha; }

    void setDensity(float density);
    float computeMass() const;
    void setMass(float mass);

    float getDensity() const { return _density; }

    glm::vec3 getVelocityInDomainUnits() const { return _velocity / (float)TREE_SCALE; } /// velocity in domain scale units (0.0-1.0) per second
    const glm::vec3 getVelocityInMeters() const { return _velocity; } /// get velocity in meters
    void setVelocityInDomainUnits(const glm::vec3& value) { _velocity = value * (float)TREE_SCALE; } /// velocity in domain scale units (0.0-1.0) per second
    void setVelocityInMeters(const glm::vec3& value) { _velocity = value; } /// velocity in meters
    bool hasVelocity() const { return _velocity != ENTITY_ITEM_ZERO_VEC3; }

    glm::vec3 getGravityInDomainUnits() const { return _gravity / (float)TREE_SCALE; } /// gravity in domain scale units (0.0-1.0) per second squared
    const glm::vec3& getGravityInMeters() const { return _gravity; } /// get gravity in meters
    void setGravityInDomainUnits(const glm::vec3& value) { _gravity = value * (float)TREE_SCALE; } /// gravity in domain scale units (0.0-1.0) per second squared
    void setGravityInMeters(const glm::vec3& value) { _gravity = value; } /// gravity in meters
    bool hasGravity() const { return _gravity != ENTITY_ITEM_ZERO_VEC3; }
    
    float getDamping() const { return _damping; }
    void setDamping(float value) { _damping = value; }

    // lifetime related properties.
    float getLifetime() const { return _lifetime; } /// get the lifetime in seconds for the entity
    void setLifetime(float value) { _lifetime = value; } /// set the lifetime in seconds for the entity

    /// is this entity immortal, in that it has no lifetime set, and will exist until manually deleted
    bool isImmortal() const { return _lifetime == ENTITY_ITEM_IMMORTAL_LIFETIME; }

    /// is this entity mortal, in that it has a lifetime set, and will automatically be deleted when that lifetime expires
    bool isMortal() const { return _lifetime != ENTITY_ITEM_IMMORTAL_LIFETIME; }
    
    /// age of this entity in seconds
    float getAge() const { return (float)(usecTimestampNow() - _created) / (float)USECS_PER_SECOND; }
    bool lifetimeHasExpired() const;
    quint64 getExpiry() const;

    // position, size, and bounds related helpers
    AACube getMaximumAACube() const;
    AACube getMinimumAACube() const;
    AABox getAABoxInMeters() const; /// axis aligned bounding box in world-frame (meters)
    AABox getAABoxInDomainUnits() const; /// axis aligned bounding box in domain scale units (0.0 - 1.0)

    const QString& getScript() const { return _script; }
    void setScript(const QString& value) { _script = value; }

    const glm::vec3& getRegistrationPoint() const { return _registrationPoint; } /// registration point as ratio of entity

    /// registration point as ratio of entity
    void setRegistrationPoint(const glm::vec3& value) 
            { _registrationPoint = glm::clamp(value, 0.0f, 1.0f); }

    const glm::vec3& getAngularVelocity() const { return _angularVelocity; }
    void setAngularVelocity(const glm::vec3& value) { _angularVelocity = value; }
    bool hasAngularVelocity() const { return _angularVelocity != ENTITY_ITEM_ZERO_VEC3; }

    float getAngularDamping() const { return _angularDamping; }
    void setAngularDamping(float value) { _angularDamping = value; }

    bool getVisible() const { return _visible; }
    void setVisible(bool value) { _visible = value; }
    bool isVisible() const { return _visible; }
    bool isInvisible() const { return !_visible; }

    bool getIgnoreForCollisions() const { return _ignoreForCollisions; }
    void setIgnoreForCollisions(bool value) { _ignoreForCollisions = value; }

    bool getCollisionsWillMove() const { return _collisionsWillMove; }
    void setCollisionsWillMove(bool value) { _collisionsWillMove = value; }

    bool getLocked() const { return _locked; }
    void setLocked(bool value) { _locked = value; }
    
    const QString& getUserData() const { return _userData; }
    void setUserData(const QString& value) { _userData = value; }
    
    // TODO: get rid of users of getRadius()... 
    float getRadiusInMeters() const;
    
    virtual bool containsInMeters(const glm::vec3& point) const { return getAABoxInMeters().contains(point); }
    virtual bool containsInDomainUnits(const glm::vec3& point) const { return getAABoxInDomainUnits().contains(point); }
    virtual void computeShapeInfo(ShapeInfo& info) const;

    /// return preferred shape type (actual physical shape may differ)
    virtual ShapeType getShapeType() const { return SHAPE_TYPE_NONE; }

    // updateFoo() methods to be used when changes need to be accumulated in the _dirtyFlags
    void updatePositionInDomainUnits(const glm::vec3& value);
    void updatePositionInMeters(const glm::vec3& value);
    void updateDimensionsInDomainUnits(const glm::vec3& value);
    void updateDimensionsInMeters(const glm::vec3& value);
    void updateRotation(const glm::quat& rotation);
    void updateDensity(float value);
    void updateMass(float value);
    void updateVelocityInDomainUnits(const glm::vec3& value);
    void updateVelocityInMeters(const glm::vec3& value);
    void updateDamping(float value);
    void updateGravityInDomainUnits(const glm::vec3& value);
    void updateGravityInMeters(const glm::vec3& value);
    void updateAngularVelocity(const glm::vec3& value);
    void updateAngularDamping(float value);
    void updateIgnoreForCollisions(bool value);
    void updateCollisionsWillMove(bool value);
    void updateLifetime(float value);
    virtual void updateShapeType(ShapeType type) { /* do nothing */ }

    uint32_t getDirtyFlags() const { return _dirtyFlags; }
    void clearDirtyFlags(uint32_t mask = 0xffff) { _dirtyFlags &= ~mask; }
    
    bool isMoving() const;

    void* getPhysicsInfo() const { return _physicsInfo; }
    void setPhysicsInfo(void* data) { _physicsInfo = data; }
    
    EntityTreeElement* getElement() const { return _element; }

    static void setSendPhysicsUpdates(bool value) { _sendPhysicsUpdates = value; }
    static bool getSendPhysicsUpdates() { return _sendPhysicsUpdates; }


protected:

    static bool _sendPhysicsUpdates;

    virtual void initFromEntityItemID(const EntityItemID& entityItemID); // maybe useful to allow subclasses to init

    EntityTypes::EntityType _type;
    QUuid _id;
    uint32_t _creatorTokenID;
    bool _newlyCreated;
    quint64 _lastSimulated; // last time this entity called simulate(), this includes velocity, angular velocity, and physics changes
    quint64 _lastUpdated; // last time this entity called update(), this includes animations and non-physics changes
    quint64 _lastEdited; // last official local or remote edit time

    quint64 _lastEditedFromRemote; // last time we received and edit from the server
    quint64 _lastEditedFromRemoteInRemoteTime; // last time we received and edit from the server (in server-time-frame)
    quint64 _created;
    quint64 _changedOnServer;

    glm::vec3 _position;
    glm::vec3 _dimensions;
    glm::quat _rotation;
    float _glowLevel;
    float _localRenderAlpha;
    float _density = ENTITY_ITEM_DEFAULT_DENSITY; // kg/m^3
    // NOTE: _volumeMultiplier is used to allow some mass properties code exist in the EntityItem base class 
    // rather than in all of the derived classes.  If we ever collapse these classes to one we could do it a
    // different way.
    float _volumeMultiplier = 1.0f;
    glm::vec3 _velocity;
    glm::vec3 _gravity;
    float _damping;
    float _lifetime;
    QString _script;
    glm::vec3 _registrationPoint;
    glm::vec3 _angularVelocity;
    float _angularDamping;
    bool _visible;
    bool _ignoreForCollisions;
    bool _collisionsWillMove;
    bool _locked;
    QString _userData;

    // NOTE: Damping is applied like this:  v *= pow(1 - damping, dt)
    //
    // Hence the damping coefficient must range from 0 (no damping) to 1 (immediate stop).
    // Each damping value relates to a corresponding exponential decay timescale as follows:
    //
    // timescale = -1 / ln(1 - damping)
    //
    // damping = 1 - exp(-1 / timescale)
    //
    
    // NOTE: Radius support is obsolete, but these private helper functions are available for this class to 
    //       parse old data streams
    
    /// set radius in domain scale units (0.0 - 1.0) this will also reset dimensions to be equal for each axis
    void setRadius(float value); 

    // _physicsInfo is a hook reserved for use by the EntitySimulation, which is guaranteed to set _physicsInfo 
    // to a non-NULL value when the EntityItem has a representation in the physics engine.
    void* _physicsInfo = NULL; // only set by EntitySimulation

    // DirtyFlags are set whenever a property changes that the EntitySimulation needs to know about.
    uint32_t _dirtyFlags;   // things that have changed from EXTERNAL changes (via script or packet) but NOT from simulation

    EntityTreeElement* _element;    // back pointer to containing Element
};



#endif // hifi_EntityItem_h
